# sm-OSINT

## Hosted-data protection

Stored usernames are protected at the database boundary with **AES-256-GCM**. Each field has a fresh random nonce and an authentication tag, so modified ciphertext is rejected. The database stores only ciphertext plus a keyed SHA-256 lookup digest of the normalized username; the digest preserves uniqueness checks without exposing a plaintext normalized username.

Set `SM_OSINT_ENCRYPTION_KEY` to a 32-byte key written as 64 random hexadecimal characters (generate one with `openssl rand -hex 32`). Keep it in a deployment secret manager or environment, never in the repository or database. Losing the key makes existing encrypted data unrecoverable; changing it requires a deliberate decrypt-and-reencrypt migration.

`database/schema.sql` is for new installations. Existing plaintext `usernames` tables need a controlled migration before this version can read them.

## Local workflow

After starting MySQL and exporting the database credentials and `SM_OSINT_ENCRYPTION_KEY`, use the local `sm-osint` executable as one pipeline:

```bash
./sm-osint --add Honda.Galmad
./sm-osint --query honda 10
./sm-osint --stats
```

It encrypts a new record before storage, decrypts records only in process, builds the prefix trie, trains the probability model, and ranks generated username candidates. `account-checker` remains a separate executable for inspecting an individual public profile URL.
