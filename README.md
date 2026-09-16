# sm-OSINT

## Hosted-data protection

Stored usernames are protected at the database boundary with **AES-256-GCM**. Each field has a fresh random nonce and an authentication tag, so modified ciphertext is rejected. The database stores only ciphertext plus a keyed SHA-256 lookup digest of the normalized username; the digest preserves uniqueness checks without exposing a plaintext normalized username.

Set `SM_OSINT_ENCRYPTION_KEY` to a 32-byte key written as 64 random hexadecimal characters (generate one with `openssl rand -hex 32`). Keep it in a deployment secret manager or environment, never in the repository or database. Losing the key makes existing encrypted data unrecoverable; changing it requires a deliberate decrypt-and-reencrypt migration.

`database/schema.sql` is for new installations. Existing plaintext `usernames` tables need a controlled migration before this version can read them.

## Local workflow

After starting MySQL and exporting the database credentials and `SM_OSINT_ENCRYPTION_KEY`, use the local `sm-osint` executable as one pipeline:

```bash
./build/sm-osint --add Honda.Galmad
./build/sm-osint --query honda 10
./build/sm-osint --generate "Honda Galmad" 20
./build/sm-osint --stats
```

It encrypts a new record before storage, decrypts records only in process, builds the prefix trie, trains the probability model, and ranks generated username candidates. `--query` searches the stored trie and displays exact and prefix occurrence counts. `--generate` works without database credentials and produces normalized username variations from a name or username; `--query` additionally ranks those variations using patterns learned from stored records.

## Terminal UI and public profile checks

Build both executables, then start the TUI with `python3 tui.py`. Press `e` to
enter one username and select **Run complete pipeline**. The TUI runs the local
encrypted-data discovery pipeline and then passes the same username to the
account-checker, which checks the supported public profile routes (Facebook,
GitHub, Instagram, X, Threads, and Codeforces). Results labelled `UNKNOWN` are
inconclusive—typically a login wall, rate limit, or anti-bot response—not a
claim that a profile is absent.

The checker can also be used outside the TUI:

```bash
./build/account-checker --username Honda.Galmad
./build/account-checker https://github.com/Honda-Galmad
```

Set `SM_OSINT_BIN` and `SM_OSINT_CHECKER_BIN` if the executables are not in
`./build`. The TUI automatically reads the project `.env` file for database
credentials and the encryption key (while exported environment values take
precedence), so it does not repeatedly fail with a missing database password.
Use Page Up/Page Down to scroll the complete platform-check output. The X API
lookup is used when `X_BEARER_TOKEN` is set; otherwise the checker uses
public-page analysis.
