# Open Source Intelligence (sm-OSINT)
### What is OSINT?
**Open Source Intelligence (OSINT)** refers to the collection, analysis, and execution of actionable intelligence derived from publicly available data sources. It encompasses data gathered across public web profiles, social network signatures, indexed public search results, and open databases.

### Why is OSINT Needed?
* **Security & Footprint Analysis:** Enables security researchers and organization defenses to audit exposure profiles, identify leaked assets, and map digital attack surfaces before malicious actors exploit them.
* **Threat & Identity Verification:** Facilitates automated verification of usernames, aliases, and public footprints across online platforms.
* **Data Integration:** Correlates disparate public platform signals into unified threat intelligence feeds.

### Project Goals
`sm-OSINT` provides a high-performance, secure local engine for managing, normalizing, analyzing, and verifying username records across standard web platforms. The application achieves this through:
1. **Cryptographically Secure Storage:** Zero plaintext exposure at rest using field-level authenticated encryption paired with deterministic lookup digests.
2. **Efficient Indexing & Probabilistic Modeling:** Native C++ implementations (utilizing dynamic dynamic data structures like Tries and Probabilistic Systems) to ingest large datasets, compute Levenshtein distance, normalize variants, and score username candidate distributions.
3. **Multi-Platform Verification:** Automated profile discovery pipelines targeting supported platform public routes.

---

## Resources & Documentation

* **NIST Special Publication 800-38D:** *Recommendation for Block Cipher Modes of Operation: Galois/Counter Mode (GCM)* — background on standard AES-256-GCM field encryption.
* **OWASP OSINT Reconnaissance Guide:** Best practices for web profiling and automated surface auditing.
* **C++ Core Guidelines:** Standards adhered to within the internal string utility, Trie, and encryption implementation source modules.

---

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
