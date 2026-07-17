#include "App_BatMan_Internal.h"

#include <stddef.h>
#include <stdio.h>

#include "Com_SOH.h"
#include "Int_EEPROM.h"

enum
{
    APP_BATMAN_NVM_SLOT_A_ADDR = 0x0000u,
    APP_BATMAN_NVM_SLOT_B_ADDR = 0x0040u,
    APP_BATMAN_NVM_RECORD_SIZE = 64u,
    APP_BATMAN_NVM_USED_SIZE = 52u,
    APP_BATMAN_NVM_CRC_OFFSET = 48u,
    APP_BATMAN_NVM_FORMAT_VERSION = 2u,
    APP_BATMAN_NVM_PERIODIC_SAVE_MS = 1800000u,
    APP_BATMAN_NVM_RECONNECT_PERIOD_MS = 10000u,
    APP_BATMAN_NVM_STARTUP_RESTORE_WINDOW_MS = 60000u,
    APP_BATMAN_NVM_READ_RETRY_COUNT = 3u,
    APP_BATMAN_NVM_FLUSH_RETRY_COUNT = 3u
};

#define APP_BATMAN_NVM_MAGIC (0x32484F53u) /* little-endian "SOH2" */

static bool s_nvm_ready = false;
static bool s_have_saved_state = false;
static uint8_t s_active_slot = 0u;
static uint32_t s_sequence = 0u;
static uint32_t s_save_elapsed_ms = 0u;
static uint32_t s_reconnect_elapsed_ms = 0u;
static uint32_t s_startup_restore_elapsed_ms = 0u;
static bool s_force_save = false;
static bool s_startup_restore_pending = false;
static Com_SOH_PersistentTypeDef s_last_saved_state;

static uint16_t App_BatMan_NvmReadU16(const uint8_t *data)
{
    return (uint16_t)(((uint16_t)data[1] << 8u) | data[0]);
}

static uint32_t App_BatMan_NvmReadU32(const uint8_t *data)
{
    return ((uint32_t)data[0]) |
           ((uint32_t)data[1] << 8u) |
           ((uint32_t)data[2] << 16u) |
           ((uint32_t)data[3] << 24u);
}

static void App_BatMan_NvmWriteU16(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)(value & 0xFFu);
    data[1] = (uint8_t)(value >> 8u);
}

static void App_BatMan_NvmWriteU32(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)(value & 0xFFu);
    data[1] = (uint8_t)((value >> 8u) & 0xFFu);
    data[2] = (uint8_t)((value >> 16u) & 0xFFu);
    data[3] = (uint8_t)(value >> 24u);
}

static uint32_t App_BatMan_NvmCrc32(const uint8_t *data, uint16_t len)
{
    uint32_t crc = 0xFFFFFFFFu;

    for (uint16_t i = 0u; i < len; i++)
    {
        crc ^= data[i];
        for (uint8_t bit = 0u; bit < 8u; bit++)
        {
            crc = ((crc & 1u) != 0u) ?
                  ((crc >> 1u) ^ 0xEDB88320u) :
                  (crc >> 1u);
        }
    }

    return crc ^ 0xFFFFFFFFu;
}

static void App_BatMan_NvmEncode(uint8_t record[APP_BATMAN_NVM_RECORD_SIZE],
                                 const Com_SOH_PersistentTypeDef *state,
                                 uint32_t sequence)
{
    for (uint8_t i = 0u; i < APP_BATMAN_NVM_RECORD_SIZE; i++)
    {
        record[i] = 0u;
    }

    App_BatMan_NvmWriteU32(&record[0], APP_BATMAN_NVM_MAGIC);
    App_BatMan_NvmWriteU16(&record[4], APP_BATMAN_NVM_FORMAT_VERSION);
    App_BatMan_NvmWriteU16(&record[6], APP_BATMAN_NVM_USED_SIZE);
    App_BatMan_NvmWriteU32(&record[8], sequence);
    App_BatMan_NvmWriteU32(&record[12], state->charge_throughput_mah);
    App_BatMan_NvmWriteU32(&record[16], state->discharge_throughput_mah);
    App_BatMan_NvmWriteU32(&record[20], state->cycle_count);
    App_BatMan_NvmWriteU32(&record[24], state->safety_fault_count);
    App_BatMan_NvmWriteU32(&record[28], state->cycle_remainder_mah);
    /* Byte 32~35、46~47 保留为 0，兼容早期 V2 未完成学习字段。 */
    App_BatMan_NvmWriteU32(&record[36], state->learned_capacity_mah);
    App_BatMan_NvmWriteU16(&record[40], state->capacity_learning_count);
    App_BatMan_NvmWriteU16(&record[42], state->max_delta_mv);
    App_BatMan_NvmWriteU16(&record[44], (uint16_t)state->max_temp_c);
    App_BatMan_NvmWriteU32(&record[APP_BATMAN_NVM_CRC_OFFSET],
                           App_BatMan_NvmCrc32(record, APP_BATMAN_NVM_CRC_OFFSET));
}

static bool App_BatMan_NvmDecode(const uint8_t record[APP_BATMAN_NVM_RECORD_SIZE],
                                 Com_SOH_PersistentTypeDef *state,
                                 uint32_t *sequence)
{
    uint32_t stored_crc;
    uint16_t flags;

    if ((App_BatMan_NvmReadU32(&record[0]) != APP_BATMAN_NVM_MAGIC) ||
        (App_BatMan_NvmReadU16(&record[4]) != APP_BATMAN_NVM_FORMAT_VERSION) ||
        (App_BatMan_NvmReadU16(&record[6]) != APP_BATMAN_NVM_USED_SIZE))
    {
        return false;
    }

    stored_crc = App_BatMan_NvmReadU32(&record[APP_BATMAN_NVM_CRC_OFFSET]);
    if (stored_crc != App_BatMan_NvmCrc32(record, APP_BATMAN_NVM_CRC_OFFSET))
    {
        return false;
    }

    flags = App_BatMan_NvmReadU16(&record[46]);
    if ((flags & (uint16_t)~0x0001u) != 0u)
    {
        return false;
    }

    *sequence = App_BatMan_NvmReadU32(&record[8]);
    state->charge_throughput_mah = App_BatMan_NvmReadU32(&record[12]);
    state->discharge_throughput_mah = App_BatMan_NvmReadU32(&record[16]);
    state->cycle_count = App_BatMan_NvmReadU32(&record[20]);
    state->safety_fault_count = App_BatMan_NvmReadU32(&record[24]);
    state->cycle_remainder_mah = App_BatMan_NvmReadU32(&record[28]);
    /* 旧 V2 的未完成学习进度只做格式兼容，不恢复到算法状态。 */
    state->learned_capacity_mah = App_BatMan_NvmReadU32(&record[36]);
    state->capacity_learning_count = App_BatMan_NvmReadU16(&record[40]);
    state->max_delta_mv = App_BatMan_NvmReadU16(&record[42]);
    state->max_temp_c = (int16_t)App_BatMan_NvmReadU16(&record[44]);
    return true;
}

static bool App_BatMan_NvmStateEqual(const Com_SOH_PersistentTypeDef *left,
                                     const Com_SOH_PersistentTypeDef *right)
{
    return (left->charge_throughput_mah == right->charge_throughput_mah) &&
           (left->discharge_throughput_mah == right->discharge_throughput_mah) &&
           (left->cycle_count == right->cycle_count) &&
           (left->safety_fault_count == right->safety_fault_count) &&
           (left->cycle_remainder_mah == right->cycle_remainder_mah) &&
           (left->learned_capacity_mah == right->learned_capacity_mah) &&
           (left->capacity_learning_count == right->capacity_learning_count) &&
           (left->max_delta_mv == right->max_delta_mv) &&
           (left->max_temp_c == right->max_temp_c);
}

static bool App_BatMan_NvmSequenceNewer(uint32_t left, uint32_t right)
{
    uint32_t delta = left - right;

    return (left != right) && (delta < 0x80000000u);
}

static bool App_BatMan_NvmReadSlot(uint16_t address,
                                   Com_SOH_PersistentTypeDef *state,
                                   uint32_t *sequence,
                                   bool *read_ok)
{
    uint8_t record[APP_BATMAN_NVM_RECORD_SIZE];
    uint8_t retry;

    *read_ok = false;
    for (retry = 0u; retry < APP_BATMAN_NVM_READ_RETRY_COUNT; retry++)
    {
        if (Int_EEPROM_Read(address, record, sizeof(record)) == INT_EEPROM_OK)
        {
            *read_ok = true;
            return App_BatMan_NvmDecode(record, state, sequence) &&
                   Com_SOH_ValidatePersistent(state);
        }

        /* 瞬态 NACK 不应直接放弃一份可能完好的历史 SOH。 */
        (void)Int_EEPROM_IsReady(INT_EEPROM_READY_TIMEOUT_MS);
    }

    return false;
}

static bool App_BatMan_NvmRestoreSlot(uint8_t slot,
                                      const Com_SOH_PersistentTypeDef *state,
                                      uint32_t sequence)
{
    if (!Com_SOH_RestorePersistent(state))
    {
        return false;
    }

    s_active_slot = slot;
    s_sequence = sequence;
    s_last_saved_state = *state;
    s_have_saved_state = true;
    return true;
}

static bool App_BatMan_NvmSave(void)
{
    uint8_t record[APP_BATMAN_NVM_RECORD_SIZE];
    uint8_t verify_record[APP_BATMAN_NVM_RECORD_SIZE];
    Com_SOH_PersistentTypeDef state;
    Com_SOH_PersistentTypeDef verify_state;
    uint32_t next_sequence = s_sequence + 1u;
    uint32_t verify_sequence;
    uint8_t next_slot = s_have_saved_state ? (uint8_t)(s_active_slot ^ 1u) : 0u;
    uint16_t address = (next_slot == 0u) ?
                       APP_BATMAN_NVM_SLOT_A_ADDR :
                       APP_BATMAN_NVM_SLOT_B_ADDR;

    Com_SOH_ExportPersistent(&state);
    if (!s_force_save &&
        s_have_saved_state &&
        App_BatMan_NvmStateEqual(&state, &s_last_saved_state))
    {
        s_save_elapsed_ms = 0u;
        return true;
    }

    App_BatMan_NvmEncode(record, &state, next_sequence);
    if (Int_EEPROM_Write(address, record, sizeof(record)) != INT_EEPROM_OK)
    {
        return false;
    }
    if (Int_EEPROM_Read(address, verify_record, sizeof(verify_record)) != INT_EEPROM_OK)
    {
        return false;
    }
    if (!App_BatMan_NvmDecode(verify_record, &verify_state, &verify_sequence) ||
        !Com_SOH_ValidatePersistent(&verify_state) ||
        (verify_sequence != next_sequence) ||
        !App_BatMan_NvmStateEqual(&state, &verify_state))
    {
        return false;
    }

    s_active_slot = next_slot;
    s_sequence = next_sequence;
    s_last_saved_state = state;
    s_have_saved_state = true;
    s_save_elapsed_ms = 0u;
    s_force_save = false;
    return true;
}

static bool App_BatMan_NvmReconnect(void)
{
    Com_SOH_PersistentTypeDef slot_a;
    Com_SOH_PersistentTypeDef slot_b;
    uint32_t sequence_a = 0u;
    uint32_t sequence_b = 0u;
    bool read_a_ok;
    bool read_b_ok;
    bool valid_a;
    bool valid_b;

    if (Int_EEPROM_Init() != INT_EEPROM_OK)
    {
        return false;
    }

    valid_a = App_BatMan_NvmReadSlot(APP_BATMAN_NVM_SLOT_A_ADDR,
                                     &slot_a,
                                     &sequence_a,
                                     &read_a_ok);
    valid_b = App_BatMan_NvmReadSlot(APP_BATMAN_NVM_SLOT_B_ADDR,
                                     &slot_b,
                                     &sequence_b,
                                     &read_b_ok);
    if (!read_a_ok || !read_b_ok)
    {
        return false;
    }

    /*
     * 若启动阶段任一槽未完成读取，首次完整重连时重新按序号恢复最新历史，
     * 不能把启动后的空白或较旧 RAM 以更高序号覆盖它。运行期掉线不走此分支。
     */
    if (s_startup_restore_pending && (valid_a || valid_b))
    {
        bool restored;

        if (valid_a && (!valid_b || App_BatMan_NvmSequenceNewer(sequence_a, sequence_b)))
        {
            restored = App_BatMan_NvmRestoreSlot(0u, &slot_a, sequence_a);
        }
        else
        {
            restored = App_BatMan_NvmRestoreSlot(1u, &slot_b, sequence_b);
        }

        if (restored)
        {
            s_startup_restore_pending = false;
            s_nvm_ready = true;
            s_force_save = false;
            printf("SOH持久化: 启动重连已恢复历史记录\r\n");
            return true;
        }
    }
    s_startup_restore_pending = false;

    s_have_saved_state = valid_a || valid_b;
    if (valid_a && (!valid_b || App_BatMan_NvmSequenceNewer(sequence_a, sequence_b)))
    {
        s_active_slot = 0u;
        s_sequence = sequence_a;
        s_last_saved_state = slot_a;
    }
    else if (valid_b)
    {
        s_active_slot = 1u;
        s_sequence = sequence_b;
        s_last_saved_state = slot_b;
    }
    else
    {
        s_active_slot = 0u;
        s_sequence = 0u;
    }

    /* 运行中重连不能用旧 EEPROM 覆盖当前 RAM，只把当前状态写成下一序号。 */
    s_nvm_ready = true;
    s_force_save = true;
    if (!App_BatMan_NvmSave())
    {
        s_nvm_ready = false;
        return false;
    }
    return true;
}

bool App_BatMan_NvmInit(void)
{
    Com_SOH_PersistentTypeDef slot_a;
    Com_SOH_PersistentTypeDef slot_b;
    uint32_t sequence_a = 0u;
    uint32_t sequence_b = 0u;
    bool valid_a;
    bool valid_b;
    bool read_a_ok;
    bool read_b_ok;

    s_nvm_ready = (Int_EEPROM_Init() == INT_EEPROM_OK);
    s_have_saved_state = false;
    s_active_slot = 0u;
    s_sequence = 0u;
    s_save_elapsed_ms = 0u;
    s_reconnect_elapsed_ms = 0u;
    s_startup_restore_elapsed_ms = 0u;
    s_force_save = false;
    s_startup_restore_pending = false;
    if (!s_nvm_ready)
    {
        s_startup_restore_pending = true;
        return false;
    }

    valid_a = App_BatMan_NvmReadSlot(APP_BATMAN_NVM_SLOT_A_ADDR,
                                     &slot_a,
                                     &sequence_a,
                                     &read_a_ok);
    valid_b = App_BatMan_NvmReadSlot(APP_BATMAN_NVM_SLOT_B_ADDR,
                                     &slot_b,
                                     &sequence_b,
                                     &read_b_ok);
    /* 单槽读取失败时仍允许从另一份已验证记录恢复，但暂不开放写入。 */
    s_nvm_ready = read_a_ok && read_b_ok;

    if (valid_a && valid_b)
    {
        /* 两个槽都通过 CRC、格式和业务字段校验时，优先恢复序号较新者。 */
        if (App_BatMan_NvmSequenceNewer(sequence_a, sequence_b))
        {
            if (!App_BatMan_NvmRestoreSlot(0u, &slot_a, sequence_a))
            {
                (void)App_BatMan_NvmRestoreSlot(1u, &slot_b, sequence_b);
            }
        }
        else
        {
            if (!App_BatMan_NvmRestoreSlot(1u, &slot_b, sequence_b))
            {
                (void)App_BatMan_NvmRestoreSlot(0u, &slot_a, sequence_a);
            }
        }
    }
    else if (valid_a)
    {
        (void)App_BatMan_NvmRestoreSlot(0u, &slot_a, sequence_a);
    }
    else if (valid_b)
    {
        (void)App_BatMan_NvmRestoreSlot(1u, &slot_b, sequence_b);
    }

    /*
     * 任一槽启动读取失败时都保留恢复窗口：未读到的槽可能比当前已恢复槽更新，
     * 完整重连后再按序号选最新记录，避免用较旧 RAM 覆盖较新历史。
     */
    if (!read_a_ok || !read_b_ok)
    {
        s_startup_restore_pending = true;
    }

    return s_nvm_ready;
}

void App_BatMan_NvmTask(uint32_t interval_ms)
{
    Com_SOH_PersistentTypeDef current;
    bool important_change = false;

    if (!s_nvm_ready)
    {
        if (s_startup_restore_pending)
        {
            if (s_startup_restore_elapsed_ms > (UINT32_MAX - interval_ms))
            {
                s_startup_restore_elapsed_ms = UINT32_MAX;
            }
            else
            {
                s_startup_restore_elapsed_ms += interval_ms;
            }
        }

        if (s_reconnect_elapsed_ms > (UINT32_MAX - interval_ms))
        {
            s_reconnect_elapsed_ms = UINT32_MAX;
        }
        else
        {
            s_reconnect_elapsed_ms += interval_ms;
        }

        if (s_reconnect_elapsed_ms >= APP_BATMAN_NVM_RECONNECT_PERIOD_MS)
        {
            s_reconnect_elapsed_ms = 0u;
            if (!App_BatMan_NvmReconnect())
            {
                printf("SOH持久化重连失败\r\n");
            }
        }

        /*
         * 只在上电后的短窗口内允许 EEPROM 覆盖 RAM。超过该窗口说明系统已经
         * 独立运行，后续重连必须以当前 RAM 为权威，避免回滚累计量和新学习结果。
         */
        if (s_startup_restore_pending &&
            (s_startup_restore_elapsed_ms >= APP_BATMAN_NVM_STARTUP_RESTORE_WINDOW_MS))
        {
            s_startup_restore_pending = false;
            printf("SOH持久化: 启动恢复窗口超时，后续以 RAM 状态为准\r\n");
        }
        return;
    }

    if (s_save_elapsed_ms > (UINT32_MAX - interval_ms))
    {
        s_save_elapsed_ms = UINT32_MAX;
    }
    else
    {
        s_save_elapsed_ms += interval_ms;
    }

    Com_SOH_ExportPersistent(&current);
    if (s_have_saved_state)
    {
        important_change = (current.cycle_count != s_last_saved_state.cycle_count) ||
                           (current.safety_fault_count != s_last_saved_state.safety_fault_count) ||
                           (current.learned_capacity_mah != s_last_saved_state.learned_capacity_mah) ||
                           (current.capacity_learning_count != s_last_saved_state.capacity_learning_count);
    }
    else
    {
        /* 首个已完成学习或累计事件立即建立有效槽，避免等到 30 分钟周期保存。 */
        important_change = (current.capacity_learning_count > 0u) ||
                           (current.cycle_count > 0u) ||
                           (current.safety_fault_count > 0u);
    }

    if (s_force_save || s_soh_capacity_updated || important_change ||
        (s_save_elapsed_ms >= APP_BATMAN_NVM_PERIODIC_SAVE_MS))
    {
        if (!App_BatMan_NvmSave())
        {
            printf("SOH持久化写入失败\r\n");
            s_nvm_ready = false;
            s_reconnect_elapsed_ms = 0u;
            s_force_save = true;
        }
    }
}

bool App_BatMan_NvmFlush(void)
{
    uint8_t retry;

    if (!s_nvm_ready)
    {
        /*
         * 关机前再做一次有界重连。启动恢复窗口内仍按“先恢复历史”处理；
         * 运行期掉线则由 Reconnect 把当前 RAM 写成新记录。
         */
        if (!App_BatMan_NvmReconnect())
        {
            return false;
        }
    }

    for (retry = 0u; retry < APP_BATMAN_NVM_FLUSH_RETRY_COUNT; retry++)
    {
        if (App_BatMan_NvmSave())
        {
            return true;
        }
        (void)Int_EEPROM_IsReady(INT_EEPROM_READY_TIMEOUT_MS);
    }

    s_nvm_ready = false;
    s_force_save = true;
    return false;
}
