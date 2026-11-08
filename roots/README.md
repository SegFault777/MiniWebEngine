# Trusted roots

| File | Content |
|---|---|
| `mozilla-certifi-2026.10.19.pem` | the current Mozilla root bundle (python-certifi 2026.10.19), the *core* tier |
| `legacy.pem` | roots of an older Mozilla vintage that are no longer in the current bundle, pre-filtered; the *legacy* tier |
| `ROOTS.txt` | every compiled-in root: SHA-256, tier, key type, name |
| `SKIPPED.txt` | roots that cannot be used (ECDSA P-521, Ed25519, RSA > 4096) |
| `LEGACY_EXCLUDED.txt` | older-vintage roots deliberately NOT included, with the reason (expired; Entrust/AffirmTrust/Chunghwa distrust-after) |
| `REMOVED.txt` | roots of the previous trusted_roots.h that disappeared on regeneration |

Regenerate `browser/tls/trusted_roots.h` (needs `openssl` on the PATH):

    python3 tools/gen_roots.py roots/mozilla-certifi-2026.10.19.pem --legacy roots/legacy.pem \
        --removed-from browser/tls/trusted_roots.h --version "certifi 2026.10.19"

Both bundles are Mozilla data (MPL-2.0, see NOTICE.md). The legacy roots are trusted only because they
were listed once; Mozilla's reasons for removing them were not checked. `-DMB_NO_LEGACY_ROOTS` compiles
the browser with the core tier only. Update the core bundle whenever Mozilla's program changes.
