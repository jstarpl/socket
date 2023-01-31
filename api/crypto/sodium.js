import libsodium from '../external/libsodium/index.js'

// default re-exports
import * as exports from './sodium.js'
export default exports

// wait for libsodium to loaded, initialized, and ready
if (typeof globalThis?.window === 'object') {
  await libsodium.ready
}

export { libsodium }

/* eslint-disable camelcase */

// public APIs
const {
  // library constants
  SODIUM_LIBRARY_VERSION_MAJOR,
  SODIUM_LIBRARY_VERSION_MINOR,
  SODIUM_VERSION_STRING,

  // `crypto_*` APIs
  crypto_aead_chacha20poly1305_ABYTES,
  crypto_aead_chacha20poly1305_IETF_ABYTES,
  crypto_aead_chacha20poly1305_IETF_KEYBYTES,
  crypto_aead_chacha20poly1305_IETF_MESSAGEBYTES_MAX,
  crypto_aead_chacha20poly1305_IETF_NPUBBYTES,
  crypto_aead_chacha20poly1305_IETF_NSECBYTES,
  crypto_aead_chacha20poly1305_KEYBYTES,
  crypto_aead_chacha20poly1305_MESSAGEBYTES_MAX,
  crypto_aead_chacha20poly1305_NPUBBYTES,
  crypto_aead_chacha20poly1305_NSECBYTES,
  crypto_aead_chacha20poly1305_decrypt,
  crypto_aead_chacha20poly1305_decrypt_detached,
  crypto_aead_chacha20poly1305_encrypt,
  crypto_aead_chacha20poly1305_encrypt_detached,
  crypto_aead_chacha20poly1305_ietf_ABYTES,
  crypto_aead_chacha20poly1305_ietf_KEYBYTES,
  crypto_aead_chacha20poly1305_ietf_MESSAGEBYTES_MAX,
  crypto_aead_chacha20poly1305_ietf_NPUBBYTES,
  crypto_aead_chacha20poly1305_ietf_NSECBYTES,
  crypto_aead_chacha20poly1305_ietf_decrypt,
  crypto_aead_chacha20poly1305_ietf_decrypt_detached,
  crypto_aead_chacha20poly1305_ietf_encrypt,
  crypto_aead_chacha20poly1305_ietf_encrypt_detached,
  crypto_aead_chacha20poly1305_ietf_keygen,
  crypto_aead_chacha20poly1305_keygen,
  crypto_aead_xchacha20poly1305_IETF_ABYTES,
  crypto_aead_xchacha20poly1305_IETF_KEYBYTES,
  crypto_aead_xchacha20poly1305_IETF_MESSAGEBYTES_MAX,
  crypto_aead_xchacha20poly1305_IETF_NPUBBYTES,
  crypto_aead_xchacha20poly1305_IETF_NSECBYTES,
  crypto_aead_xchacha20poly1305_ietf_ABYTES,
  crypto_aead_xchacha20poly1305_ietf_KEYBYTES,
  crypto_aead_xchacha20poly1305_ietf_MESSAGEBYTES_MAX,
  crypto_aead_xchacha20poly1305_ietf_NPUBBYTES,
  crypto_aead_xchacha20poly1305_ietf_NSECBYTES,
  crypto_aead_xchacha20poly1305_ietf_decrypt,
  crypto_aead_xchacha20poly1305_ietf_decrypt_detached,
  crypto_aead_xchacha20poly1305_ietf_encrypt,
  crypto_aead_xchacha20poly1305_ietf_encrypt_detached,
  crypto_aead_xchacha20poly1305_ietf_keygen,
  crypto_auth,
  crypto_auth_BYTES,
  crypto_auth_KEYBYTES,
  crypto_auth_keygen,
  crypto_auth_verify,
  crypto_box_BEFORENMBYTES,
  crypto_box_MACBYTES,
  crypto_box_MESSAGEBYTES_MAX,
  crypto_box_NONCEBYTES,
  crypto_box_PUBLICKEYBYTES,
  crypto_box_SEALBYTES,
  crypto_box_SECRETKEYBYTES,
  crypto_box_SEEDBYTES,
  crypto_box_beforenm,
  crypto_box_detached,
  crypto_box_easy,
  crypto_box_easy_afternm,
  crypto_box_keypair,
  crypto_box_open_detached,
  crypto_box_open_easy,
  crypto_box_open_easy_afternm,
  crypto_box_seal,
  crypto_box_seal_open,
  crypto_box_seed_keypair,
  crypto_generichash,
  crypto_generichash_BYTES,
  crypto_generichash_BYTES_MAX,
  crypto_generichash_BYTES_MIN,
  crypto_generichash_KEYBYTES,
  crypto_generichash_KEYBYTES_MAX,
  crypto_generichash_KEYBYTES_MIN,
  crypto_generichash_final,
  crypto_generichash_init,
  crypto_generichash_keygen,
  crypto_generichash_update,
  crypto_hash,
  crypto_hash_BYTES,
  crypto_kdf_BYTES_MAX,
  crypto_kdf_BYTES_MIN,
  crypto_kdf_CONTEXTBYTES,
  crypto_kdf_KEYBYTES,
  crypto_kdf_derive_from_key,
  crypto_kdf_keygen,
  crypto_kx_PUBLICKEYBYTES,
  crypto_kx_SECRETKEYBYTES,
  crypto_kx_SEEDBYTES,
  crypto_kx_SESSIONKEYBYTES,
  crypto_kx_client_session_keys,
  crypto_kx_keypair,
  crypto_kx_seed_keypair,
  crypto_kx_server_session_keys,
  crypto_pwhash,
  crypto_pwhash_ALG_ARGON2I13,
  crypto_pwhash_ALG_ARGON2ID13,
  crypto_pwhash_ALG_DEFAULT,
  crypto_pwhash_BYTES_MAX,
  crypto_pwhash_BYTES_MIN,
  crypto_pwhash_MEMLIMIT_INTERACTIVE,
  crypto_pwhash_MEMLIMIT_MAX,
  crypto_pwhash_MEMLIMIT_MIN,
  crypto_pwhash_MEMLIMIT_MODERATE,
  crypto_pwhash_MEMLIMIT_SENSITIVE,
  crypto_pwhash_OPSLIMIT_INTERACTIVE,
  crypto_pwhash_OPSLIMIT_MAX,
  crypto_pwhash_OPSLIMIT_MIN,
  crypto_pwhash_OPSLIMIT_MODERATE,
  crypto_pwhash_OPSLIMIT_SENSITIVE,
  crypto_pwhash_PASSWD_MAX,
  crypto_pwhash_PASSWD_MIN,
  crypto_pwhash_SALTBYTES,
  crypto_pwhash_STRBYTES,
  crypto_pwhash_STRPREFIX,
  crypto_pwhash_str,
  crypto_pwhash_str_needs_rehash,
  crypto_pwhash_str_verify,
  crypto_scalarmult,
  crypto_scalarmult_BYTES,
  crypto_scalarmult_SCALARBYTES,
  crypto_scalarmult_base,
  crypto_secretbox_KEYBYTES,
  crypto_secretbox_MACBYTES,
  crypto_secretbox_MESSAGEBYTES_MAX,
  crypto_secretbox_NONCEBYTES,
  crypto_secretbox_detached,
  crypto_secretbox_easy,
  crypto_secretbox_keygen,
  crypto_secretbox_open_detached,
  crypto_secretbox_open_easy,
  crypto_secretstream_xchacha20poly1305_ABYTES,
  crypto_secretstream_xchacha20poly1305_HEADERBYTES,
  crypto_secretstream_xchacha20poly1305_KEYBYTES,
  crypto_secretstream_xchacha20poly1305_MESSAGEBYTES_MAX,
  crypto_secretstream_xchacha20poly1305_TAG_FINAL,
  crypto_secretstream_xchacha20poly1305_TAG_MESSAGE,
  crypto_secretstream_xchacha20poly1305_TAG_PUSH,
  crypto_secretstream_xchacha20poly1305_TAG_REKEY,
  crypto_secretstream_xchacha20poly1305_init_pull,
  crypto_secretstream_xchacha20poly1305_init_push,
  crypto_secretstream_xchacha20poly1305_keygen,
  crypto_secretstream_xchacha20poly1305_pull,
  crypto_secretstream_xchacha20poly1305_push,
  crypto_secretstream_xchacha20poly1305_rekey,
  crypto_shorthash,
  crypto_shorthash_BYTES,
  crypto_shorthash_KEYBYTES,
  crypto_shorthash_keygen,
  crypto_sign,
  crypto_sign_BYTES,
  crypto_sign_MESSAGEBYTES_MAX,
  crypto_sign_PUBLICKEYBYTES,
  crypto_sign_SECRETKEYBYTES,
  crypto_sign_SEEDBYTES,
  crypto_sign_detached,
  crypto_sign_ed25519_pk_to_curve25519,
  crypto_sign_ed25519_sk_to_curve25519,
  crypto_sign_final_create,
  crypto_sign_final_verify,
  crypto_sign_init,
  crypto_sign_keypair,
  crypto_sign_open,
  crypto_sign_seed_keypair,
  crypto_sign_update,
  crypto_sign_verify_detached,

  // `randombytes_*` APIs
  randombytes_buf,
  randombytes_buf_deterministic,
  randombytes_close,
  randombytes_random,
  randombytes_stir,
  randombytes_uniform,

  // `sodium_*` APIs
  sodium_version_string
} = libsodium

export {
  SODIUM_LIBRARY_VERSION_MAJOR,
  SODIUM_LIBRARY_VERSION_MINOR,
  SODIUM_VERSION_STRING,

  // `crypto_*` APIs
  crypto_aead_chacha20poly1305_ABYTES,
  crypto_aead_chacha20poly1305_IETF_ABYTES,
  crypto_aead_chacha20poly1305_IETF_KEYBYTES,
  crypto_aead_chacha20poly1305_IETF_MESSAGEBYTES_MAX,
  crypto_aead_chacha20poly1305_IETF_NPUBBYTES,
  crypto_aead_chacha20poly1305_IETF_NSECBYTES,
  crypto_aead_chacha20poly1305_KEYBYTES,
  crypto_aead_chacha20poly1305_MESSAGEBYTES_MAX,
  crypto_aead_chacha20poly1305_NPUBBYTES,
  crypto_aead_chacha20poly1305_NSECBYTES,
  crypto_aead_chacha20poly1305_decrypt,
  crypto_aead_chacha20poly1305_decrypt_detached,
  crypto_aead_chacha20poly1305_encrypt,
  crypto_aead_chacha20poly1305_encrypt_detached,
  crypto_aead_chacha20poly1305_ietf_ABYTES,
  crypto_aead_chacha20poly1305_ietf_KEYBYTES,
  crypto_aead_chacha20poly1305_ietf_MESSAGEBYTES_MAX,
  crypto_aead_chacha20poly1305_ietf_NPUBBYTES,
  crypto_aead_chacha20poly1305_ietf_NSECBYTES,
  crypto_aead_chacha20poly1305_ietf_decrypt,
  crypto_aead_chacha20poly1305_ietf_decrypt_detached,
  crypto_aead_chacha20poly1305_ietf_encrypt,
  crypto_aead_chacha20poly1305_ietf_encrypt_detached,
  crypto_aead_chacha20poly1305_ietf_keygen,
  crypto_aead_chacha20poly1305_keygen,
  crypto_aead_xchacha20poly1305_IETF_ABYTES,
  crypto_aead_xchacha20poly1305_IETF_KEYBYTES,
  crypto_aead_xchacha20poly1305_IETF_MESSAGEBYTES_MAX,
  crypto_aead_xchacha20poly1305_IETF_NPUBBYTES,
  crypto_aead_xchacha20poly1305_IETF_NSECBYTES,
  crypto_aead_xchacha20poly1305_ietf_ABYTES,
  crypto_aead_xchacha20poly1305_ietf_KEYBYTES,
  crypto_aead_xchacha20poly1305_ietf_MESSAGEBYTES_MAX,
  crypto_aead_xchacha20poly1305_ietf_NPUBBYTES,
  crypto_aead_xchacha20poly1305_ietf_NSECBYTES,
  crypto_aead_xchacha20poly1305_ietf_decrypt,
  crypto_aead_xchacha20poly1305_ietf_decrypt_detached,
  crypto_aead_xchacha20poly1305_ietf_encrypt,
  crypto_aead_xchacha20poly1305_ietf_encrypt_detached,
  crypto_aead_xchacha20poly1305_ietf_keygen,
  crypto_auth,
  crypto_auth_BYTES,
  crypto_auth_KEYBYTES,
  crypto_auth_keygen,
  crypto_auth_verify,
  crypto_box_BEFORENMBYTES,
  crypto_box_MACBYTES,
  crypto_box_MESSAGEBYTES_MAX,
  crypto_box_NONCEBYTES,
  crypto_box_PUBLICKEYBYTES,
  crypto_box_SEALBYTES,
  crypto_box_SECRETKEYBYTES,
  crypto_box_SEEDBYTES,
  crypto_box_beforenm,
  crypto_box_detached,
  crypto_box_easy,
  crypto_box_easy_afternm,
  crypto_box_keypair,
  crypto_box_open_detached,
  crypto_box_open_easy,
  crypto_box_open_easy_afternm,
  crypto_box_seal,
  crypto_box_seal_open,
  crypto_box_seed_keypair,
  crypto_generichash,
  crypto_generichash_BYTES,
  crypto_generichash_BYTES_MAX,
  crypto_generichash_BYTES_MIN,
  crypto_generichash_KEYBYTES,
  crypto_generichash_KEYBYTES_MAX,
  crypto_generichash_KEYBYTES_MIN,
  crypto_generichash_final,
  crypto_generichash_init,
  crypto_generichash_keygen,
  crypto_generichash_update,
  crypto_hash,
  crypto_hash_BYTES,
  crypto_kdf_BYTES_MAX,
  crypto_kdf_BYTES_MIN,
  crypto_kdf_CONTEXTBYTES,
  crypto_kdf_KEYBYTES,
  crypto_kdf_derive_from_key,
  crypto_kdf_keygen,
  crypto_kx_PUBLICKEYBYTES,
  crypto_kx_SECRETKEYBYTES,
  crypto_kx_SEEDBYTES,
  crypto_kx_SESSIONKEYBYTES,
  crypto_kx_client_session_keys,
  crypto_kx_keypair,
  crypto_kx_seed_keypair,
  crypto_kx_server_session_keys,
  crypto_pwhash,
  crypto_pwhash_ALG_ARGON2I13,
  crypto_pwhash_ALG_ARGON2ID13,
  crypto_pwhash_ALG_DEFAULT,
  crypto_pwhash_BYTES_MAX,
  crypto_pwhash_BYTES_MIN,
  crypto_pwhash_MEMLIMIT_INTERACTIVE,
  crypto_pwhash_MEMLIMIT_MAX,
  crypto_pwhash_MEMLIMIT_MIN,
  crypto_pwhash_MEMLIMIT_MODERATE,
  crypto_pwhash_MEMLIMIT_SENSITIVE,
  crypto_pwhash_OPSLIMIT_INTERACTIVE,
  crypto_pwhash_OPSLIMIT_MAX,
  crypto_pwhash_OPSLIMIT_MIN,
  crypto_pwhash_OPSLIMIT_MODERATE,
  crypto_pwhash_OPSLIMIT_SENSITIVE,
  crypto_pwhash_PASSWD_MAX,
  crypto_pwhash_PASSWD_MIN,
  crypto_pwhash_SALTBYTES,
  crypto_pwhash_STRBYTES,
  crypto_pwhash_STRPREFIX,
  crypto_pwhash_str,
  crypto_pwhash_str_needs_rehash,
  crypto_pwhash_str_verify,
  crypto_scalarmult,
  crypto_scalarmult_BYTES,
  crypto_scalarmult_SCALARBYTES,
  crypto_scalarmult_base,
  crypto_secretbox_KEYBYTES,
  crypto_secretbox_MACBYTES,
  crypto_secretbox_MESSAGEBYTES_MAX,
  crypto_secretbox_NONCEBYTES,
  crypto_secretbox_detached,
  crypto_secretbox_easy,
  crypto_secretbox_keygen,
  crypto_secretbox_open_detached,
  crypto_secretbox_open_easy,
  crypto_secretstream_xchacha20poly1305_ABYTES,
  crypto_secretstream_xchacha20poly1305_HEADERBYTES,
  crypto_secretstream_xchacha20poly1305_KEYBYTES,
  crypto_secretstream_xchacha20poly1305_MESSAGEBYTES_MAX,
  crypto_secretstream_xchacha20poly1305_TAG_FINAL,
  crypto_secretstream_xchacha20poly1305_TAG_MESSAGE,
  crypto_secretstream_xchacha20poly1305_TAG_PUSH,
  crypto_secretstream_xchacha20poly1305_TAG_REKEY,
  crypto_secretstream_xchacha20poly1305_init_pull,
  crypto_secretstream_xchacha20poly1305_init_push,
  crypto_secretstream_xchacha20poly1305_keygen,
  crypto_secretstream_xchacha20poly1305_pull,
  crypto_secretstream_xchacha20poly1305_push,
  crypto_secretstream_xchacha20poly1305_rekey,
  crypto_shorthash,
  crypto_shorthash_BYTES,
  crypto_shorthash_KEYBYTES,
  crypto_shorthash_keygen,
  crypto_sign,
  crypto_sign_BYTES,
  crypto_sign_MESSAGEBYTES_MAX,
  crypto_sign_PUBLICKEYBYTES,
  crypto_sign_SECRETKEYBYTES,
  crypto_sign_SEEDBYTES,
  crypto_sign_detached,
  crypto_sign_ed25519_pk_to_curve25519,
  crypto_sign_ed25519_sk_to_curve25519,
  crypto_sign_final_create,
  crypto_sign_final_verify,
  crypto_sign_init,
  crypto_sign_keypair,
  crypto_sign_open,
  crypto_sign_seed_keypair,
  crypto_sign_update,
  crypto_sign_verify_detached,

  // `randombytes_*` APIs
  randombytes_buf,
  randombytes_buf_deterministic,
  randombytes_close,
  randombytes_random,
  randombytes_stir,
  randombytes_uniform,

  // `sodium_*` APIs
  sodium_version_string
}