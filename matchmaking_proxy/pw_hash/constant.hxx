#ifndef EB95EBA8_1297_44AA_9C1C_A1AB22A88B30
#define EB95EBA8_1297_44AA_9C1C_A1AB22A88B30

#include <sodium.h>
namespace matchmaking_proxy
{
#ifdef DEBUG
auto const hash_memory = crypto_pwhash_argon2id_MEMLIMIT_MIN;
auto const hash_opt = crypto_pwhash_argon2id_OPSLIMIT_MIN;
#else
// TODO the library user should be able to configure this.
// increasing this will slow down login and create account but increase the time an attacker needs to figure out the passwords if the database gets stolen
auto const hash_memory = crypto_pwhash_argon2id_MEMLIMIT_MIN;
auto const hash_opt = crypto_pwhash_argon2id_OPSLIMIT_MIN;
#endif
}
#endif /* EB95EBA8_1297_44AA_9C1C_A1AB22A88B30 */
