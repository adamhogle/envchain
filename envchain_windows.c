/* envchain_windows.c - Windows Credential Manager backend for envchain
 *
 * Uses the Windows Data Protection API via CredWrite/CredRead/CredDelete/CredEnumerate.
 * Credentials are stored as CRED_TYPE_GENERIC entries with target names of the form:
 *   envchain-<namespace>/<key>
 */

#include "envchain.h"

#include <windows.h>
#include <wincred.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ENVCHAIN_PREFIX     "envchain-"
#define ENVCHAIN_PREFIX_LEN 9  /* strlen("envchain-") */

/* ---- string helpers ---- */

static wchar_t *
utf8_to_wide(const char *str)
{
  int len = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0);
  wchar_t *wstr = (wchar_t *)malloc((size_t)len * sizeof(wchar_t));
  if (wstr == NULL) { fprintf(stderr, "malloc failed\n"); exit(10); }
  MultiByteToWideChar(CP_UTF8, 0, str, -1, wstr, len);
  return wstr;
}

static char *
wide_to_utf8(const wchar_t *wstr)
{
  int len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
  char *str = (char *)malloc((size_t)len);
  if (str == NULL) { fprintf(stderr, "malloc failed\n"); exit(10); }
  WideCharToMultiByte(CP_UTF8, 0, wstr, -1, str, len, NULL, NULL);
  return str;
}

static void
envchain_fail_win32(DWORD err)
{
  char *msg = NULL;
  FormatMessageA(
    FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
    NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
    (LPSTR)&msg, 0, NULL
  );
  fprintf(stderr, "Error (%lu): %s\n", (unsigned long)err, msg ? msg : "unknown error");
  LocalFree(msg);
  exit(10);
}

/* Build a wide target name: "envchain-<name>/<key>"
 * Pass key=NULL to build a wildcard filter "envchain-<name>/*"
 */
static wchar_t *
build_target(const char *name, const char *key)
{
  wchar_t *wname = utf8_to_wide(name);
  wchar_t *wkey  = key ? utf8_to_wide(key) : NULL;

  /* prefix(9) + name + '/'(1) + key_or_star + NUL */
  size_t total = 9 + wcslen(wname) + 1 + (wkey ? wcslen(wkey) : 1) + 1;
  wchar_t *target = (wchar_t *)malloc(total * sizeof(wchar_t));
  if (target == NULL) { fprintf(stderr, "malloc failed\n"); exit(10); }

  if (wkey) {
    _snwprintf(target, total, L"envchain-%s/%s", wname, wkey);
  } else {
    _snwprintf(target, total, L"envchain-%s/*", wname);
  }

  free(wname);
  if (wkey) free(wkey);
  return target;
}

/* ---- simple insertion sort for char* arrays ---- */

static int
cmp_str(const void *a, const void *b)
{
  return strcmp(*(const char **)a, *(const char **)b);
}

/* ---- public API ---- */

int
envchain_search_namespaces(envchain_namespace_search_callback callback, void *data)
{
  DWORD count = 0;
  PCREDENTIALW *creds = NULL;

  if (!CredEnumerateW(L"envchain-*", 0, &count, &creds)) {
    DWORD err = GetLastError();
    if (err == ERROR_NOT_FOUND) return 0;
    envchain_fail_win32(err);
  }

  char **names = (char **)malloc(count * sizeof(char *));
  if (names == NULL) { CredFree(creds); fprintf(stderr, "malloc failed\n"); exit(10); }
  int name_count = 0;

  for (DWORD i = 0; i < count; i++) {
    char *target = wide_to_utf8(creds[i]->TargetName);

    if (strncmp(target, ENVCHAIN_PREFIX, ENVCHAIN_PREFIX_LEN) != 0) {
      free(target);
      continue;
    }

    char *name_part = target + ENVCHAIN_PREFIX_LEN;
    char *slash = strchr(name_part, '/');
    if (slash == NULL) { free(target); continue; }

    size_t ns_len = (size_t)(slash - name_part);
    char *ns = (char *)malloc(ns_len + 1);
    if (ns == NULL) { free(target); continue; }
    memcpy(ns, name_part, ns_len);
    ns[ns_len] = '\0';

    /* deduplicate */
    int dup = 0;
    for (int j = 0; j < name_count; j++) {
      if (strcmp(names[j], ns) == 0) { dup = 1; break; }
    }
    if (!dup) {
      names[name_count++] = ns;
    } else {
      free(ns);
    }

    free(target);
  }

  qsort(names, (size_t)name_count, sizeof(char *), cmp_str);
  for (int i = 0; i < name_count; i++) {
    callback(names[i], data);
    free(names[i]);
  }

  free(names);
  CredFree(creds);
  return 0;
}

int
envchain_search_values(const char *name, envchain_search_callback callback, void *data)
{
  DWORD count = 0;
  PCREDENTIALW *creds = NULL;
  wchar_t *filter = build_target(name, NULL); /* "envchain-<name>/*" */

  if (!CredEnumerateW(filter, 0, &count, &creds)) {
    DWORD err = GetLastError();
    free(filter);
    if (err == ERROR_NOT_FOUND) {
      fprintf(stderr,
        "WARNING: namespace `%s` not defined.\n"
        "         You can set via running `%s --set %s SOME_ENV_NAME`.\n\n",
        name, envchain_name, name
      );
      return 1;
    }
    envchain_fail_win32(err);
  }
  free(filter);

  for (DWORD i = 0; i < count; i++) {
    char *target = wide_to_utf8(creds[i]->TargetName);

    char *slash = strrchr(target, '/');
    if (slash == NULL) { free(target); continue; }
    char *key = slash + 1;

    DWORD blob_size = creds[i]->CredentialBlobSize;
    char *value = (char *)malloc(blob_size + 1);
    if (value == NULL) { free(target); continue; }
    memcpy(value, creds[i]->CredentialBlob, blob_size);
    value[blob_size] = '\0';

    callback(key, value, data);

    SecureZeroMemory(value, blob_size);
    free(value);
    free(target);
  }

  CredFree(creds);
  return 0;
}

void
envchain_save_value(const char *name, const char *key, char *value, int require_passphrase)
{
  if (require_passphrase == 1) {
    fprintf(stderr,
      "%s: Sorry, `--require-passphrase' is unsupported on Windows\n",
      envchain_name);
    return;
  }

  wchar_t *target = build_target(name, key);

  CREDENTIALW cred;
  memset(&cred, 0, sizeof(cred));
  cred.Type              = CRED_TYPE_GENERIC;
  cred.TargetName        = target;
  cred.CredentialBlob     = (LPBYTE)value;
  cred.CredentialBlobSize = (DWORD)strlen(value);
  cred.Persist           = CRED_PERSIST_LOCAL_MACHINE;

  if (!CredWriteW(&cred, 0)) {
    DWORD err = GetLastError();
    free(target);
    envchain_fail_win32(err);
  }

  free(target);
}

void
envchain_delete_value(const char *name, const char *key)
{
  wchar_t *target = build_target(name, key);

  if (!CredDeleteW(target, CRED_TYPE_GENERIC, 0)) {
    DWORD err = GetLastError();
    if (err != ERROR_NOT_FOUND) {
      free(target);
      envchain_fail_win32(err);
    }
  }

  free(target);
}
