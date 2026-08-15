#!/usr/bin/env python3
import sys

PATH = "src/daemon/shredderd.c"

EDITS = []

EDITS.append(('''#include <openssl/sha.h>''', '''#include <openssl/evp.h>'''))

EDITS.append(('''static void sha256_file(const char *path, char *out) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    FILE *f = fopen(path, "rb");
    if (!f) { strcpy(out, "UNKNOWN"); return; }
    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
        SHA256_Update(&ctx, buf, n);
    SHA256_Final(hash, &ctx);
    fclose(f);
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
        sprintf(out + i * 2, "%02x", hash[i]);
    out[64] = 0;
}''', '''static void sha256_file(const char *path, char *out) {
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len = 0;

    FILE *f = fopen(path, "rb");
    if (!f) { strcpy(out, "UNKNOWN"); return; }

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) { fclose(f); strcpy(out, "UNKNOWN"); return; }

    if (EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) != 1) {
        EVP_MD_CTX_free(ctx);
        fclose(f);
        strcpy(out, "UNKNOWN");
        return;
    }

    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
        EVP_DigestUpdate(ctx, buf, n);

    EVP_DigestFinal_ex(ctx, hash, &hash_len);
    EVP_MD_CTX_free(ctx);
    fclose(f);

    for (unsigned int i = 0; i < hash_len; i++)
        sprintf(out + i * 2, "%02x", hash[i]);
    out[hash_len * 2] = 0;
}'''))

def main():
    with open(PATH, "r") as f:
        content = f.read()

    for i, (old, new) in enumerate(EDITS):
        count = content.count(old)
        if count != 1:
            print(f"ABORT at edit {i}: expected exactly 1 match, found {count}", file=sys.stderr)
            print(f"--- OLD snippet ---\n{old[:200]}", file=sys.stderr)
            sys.exit(1)
        content = content.replace(old, new)

    with open(PATH, "w") as f:
        f.write(content)

    print(f"OK: applied {len(EDITS)} edits to {PATH}")

if __name__ == "__main__":
    main()
