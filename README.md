# RansomwareLab

An educational code-reading project for understanding **file-encryption behavior in a simplified ransomware example**. It demonstrates the use of OpenSSL with AES-256-GCM, the format of encrypted data, and the role of a key in file recovery.

> [!CAUTION]
> **The current version modifies real files; it is not a safe simulation.** When the encryption path processes files, the program creates `.locked` files and deletes the original after a successful write. It also saves the raw encryption key in `attacker_key.bin` and creates a sample ransom note. Do not run the executable or build the program on a machine that contains important files.
>
> For that reason, this README explains the project through static code review and does not provide commands to run encryption or decryption. Do not use it until it has been converted into a non-destructive simulation and the changes have been reviewed.

## Contents

- [Learning objectives](#learning-objectives)
- [How the code works](#how-the-code-works)
- [Scope](#scope)
- [Safe study workflow](#safe-study-workflow)
- [Data format](#data-format)
- [Notes before publishing on GitHub](#notes-before-publishing-on-github)

## Learning objectives

This project helps students trace the basic stages of a limited file-encryption example:

1. Identify files in a sample directory.
2. Generate a random key and a random initialization vector (IV).
3. Encrypt file contents and authenticate them with AES-256-GCM.
4. Write the output in a project-specific binary format.
5. Understand how decryption depends on the correct key and authentication tag.

This project is **not a data-protection or file-recovery tool** and should not be used on real files.

## How the code works

The program's data flow can be studied in `main.cpp` without running it:

```text
File inside lab_files
        │
        ├── Read contents into memory
        ├── Generate a random IV
        ├── Encrypt with AES-256-GCM
        ├── Write a .locked file
        └── Delete the original after a successful write
```

In the recovery path, the program reads a `.locked` file, extracts the IV and authentication tag, then attempts decryption with the saved key. If successful, it writes the recovered contents and deletes the `.locked` file.

### Implementation components

| Component | Role in the project |
|---|---|
| `random_bytes` | Requests random bytes from OpenSSL's random number generator. |
| `aes_encrypt` | Encrypts data with AES-256-GCM and retrieves the authentication tag. |
| `aes_decrypt` | Decrypts data and verifies the authentication tag. |
| `encrypt_sample` | Writes the encrypted container, then attempts to delete the temporary original file. |
| `decrypt_sample` | Recovers file contents from the encrypted container. |
| `main` | Selects the operation and iterates over files directly inside the lab directory. |

## Scope

According to the current code:

- The program looks for a `lab_files` directory under the **current working directory**.
- It processes regular files directly inside that directory; it does not traverse subdirectories.
- The encryption path skips files with the `.locked` extension.
- It creates `attacker_key.bin` next to the working directory and stores the raw key without protection.
- It creates `README_RESTORE.txt` inside `lab_files` with sample ransom text.

Storing the key beside the program makes this example an unrealistic model of attacker key management, and storing it in raw form does not protect it. Error handling is limited, so recovery is not guaranteed to succeed in every case.

## Safe study workflow

To learn from the project without running code that changes files:

1. Open `main.cpp` and read the functions listed in the implementation table.
2. Trace how data moves from file reading to encryption and container writing.
3. Review how the IV and authentication tag are stored and used during decryption.
4. Note the file-writing and deletion points (`write_file` and `fs::remove`) to understand their effects.

If a lesson requires a practical demonstration, first convert the project into a **non-destructive simulation**: use embedded test data or in-memory data, leave original files untouched, and remove the ransom note. Review those changes before running anything.

## Data format

The container written by the project begins with these fields, in order:

| Field | Size |
|---|---:|
| `LAB1` header | 4 bytes |
| IV | 12 bytes |
| Authentication tag (GCM tag) | 16 bytes |
| Ciphertext | Variable |

The authentication tag is part of AES-GCM. Using the wrong key or modifying the data will generally cause authentication to fail during decryption.

## Notes before publishing on GitHub

- Describe the repository and its topics as intended for **education and static analysis**.
- Do not attach `main.exe` as a ready-to-run download or release.
- Add a `LICENSE` file specifying the reuse terms you choose.
- If you want the project to be suitable for live classroom demonstrations, make the published version non-destructive before adding build or run instructions.
- Review the repository contents to ensure it does not include keys, personal files, or unintended test outputs.
