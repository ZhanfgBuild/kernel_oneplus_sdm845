#include <linux/err.h>
#include <linux/fs.h>
#include <linux/gfp.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/version.h>
#include <linux/sched.h>
#include <linux/pid.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
#include <linux/sched/task.h>
#endif
#ifdef CONFIG_KSU_DEBUG
#include <linux/moduleparam.h>
#endif
#include <crypto/hash.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 11, 0)
#include <crypto/sha2.h>
#else
#include <crypto/sha.h>
#endif

#include "manager/throne_tracker.h"
#include "compat/kernel_compat.h"
#include "dynamic_manager.h"
#include "klog.h" // IWYU pragma: keep
#include "manager/manager_identity.h"
#include "ksu.h"

// Dynamic sign configuration. size, hash, and is_set are one snapshot:
// writers and readers take dynamic_manager_lock around all three.
static DEFINE_MUTEX(dynamic_manager_lock);
static struct dynamic_manager_config dynamic_manager = {
    .size = 0x300,
    .hash = "0000000000000000000000000000000000000000000000000000000000000000",
    .is_set = 0
};
/* Filled under dynamic_manager_lock. Stable against writers, reused by the
 * next ksu_get_dynamic_manager_sign() call.
 */
static char dynamic_manager_hash_copy[sizeof(dynamic_manager.hash)];

bool ksu_is_dynamic_manager_enabled(void)
{
    bool enabled;

    mutex_lock(&dynamic_manager_lock);
    enabled = dynamic_manager.is_set;
    mutex_unlock(&dynamic_manager_lock);
    return enabled;
}

apk_sign_key_t ksu_get_dynamic_manager_sign(void)
{
    apk_sign_key_t sign_key;

    mutex_lock(&dynamic_manager_lock);
    sign_key.size = dynamic_manager.size;
    memcpy(dynamic_manager_hash_copy, dynamic_manager.hash,
           sizeof(dynamic_manager_hash_copy));
    mutex_unlock(&dynamic_manager_lock);
    sign_key.sha256 = dynamic_manager_hash_copy;
    return sign_key;
}

bool ksu_dynamic_manager_matches(unsigned int size, const char *hash)
{
    bool match = false;

    if (!hash)
        return false;

    mutex_lock(&dynamic_manager_lock);
    if (dynamic_manager.is_set && dynamic_manager.size == size &&
        strcmp(dynamic_manager.hash, hash) == 0)
        match = true;
    mutex_unlock(&dynamic_manager_lock);
    return match;
}

int ksu_handle_dynamic_manager(struct ksu_dynamic_manager_cmd *cmd)
{
    int ret = 0;
    int i;

    if (!cmd) {
        return -EINVAL;
    }

    switch (cmd->operation) {
    case DYNAMIC_MANAGER_OP_SET:
        if (cmd->size < 0x100 || cmd->size > 0x1000) {
            pr_err("invalid size: 0x%x\n", cmd->size);
            return -EINVAL;
        }

        // Validate hash format
        for (i = 0; i < 64; i++) {
            char c = cmd->hash[i];
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
                pr_err("invalid hash character at position %d: %c\n", i, c);
                return -EINVAL;
            }
        }

        mutex_lock(&dynamic_manager_lock);
        if (dynamic_manager.is_set)
            ksu_unregister_manager_by_signature_index(KSU_SIGNATURE_INDEX_DYNAMIC_MANAGER);

        dynamic_manager.size = cmd->size;
        // userspace always put an char[64] to our
        // we just use memcpy to copy memory, and flag [64] to \0 by ourselves
        memcpy(dynamic_manager.hash, cmd->hash, 64);
        dynamic_manager.hash[64] = '\0';
        dynamic_manager.is_set = 1;
        mutex_unlock(&dynamic_manager_lock);

        track_throne(TRACK_THRONE_FORCE_SEARCH_MGR);
        pr_info("dynamic manager updated: size=0x%x, hash=%.16s\n", cmd->size, cmd->hash);
        break;

    case DYNAMIC_MANAGER_OP_GET:
        mutex_lock(&dynamic_manager_lock);
        if (dynamic_manager.is_set) {
            cmd->size = dynamic_manager.size;
            memcpy(cmd->hash, dynamic_manager.hash,
                   64); // just copy [64] is enough, userspace will handle that
            ret = 0;
        } else {
            ret = -ENODATA;
        }
        mutex_unlock(&dynamic_manager_lock);
        break;
    case DYNAMIC_MANAGER_OP_WIPE:
        mutex_lock(&dynamic_manager_lock);
        dynamic_manager.is_set = 0;
        mutex_unlock(&dynamic_manager_lock);
        ret = 0;
        ksu_unregister_manager_by_signature_index(KSU_SIGNATURE_INDEX_DYNAMIC_MANAGER);
        pr_info("dynamic manager kernel settings reseted");
        break;

    default:
        pr_err("Invalid dynamic manager operation: %d\n", cmd->operation);
        return -EINVAL;
    }

    return ret;
}
