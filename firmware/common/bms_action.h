#ifndef BMS_ACTION_H
#define BMS_ACTION_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BMS_ACTION_NONE              (UINT32_C(0))
#define BMS_ACTION_INHIBIT_CHARGE    (UINT32_C(1) << 0u)
#define BMS_ACTION_INHIBIT_DISCHARGE (UINT32_C(1) << 1u)
#define BMS_ACTION_INHIBIT_BALANCE   (UINT32_C(1) << 2u)
#define BMS_ACTION_ASSERT_PSTOP      (UINT32_C(1) << 3u)
#define BMS_ACTION_LOCKOUT           (UINT32_C(1) << 4u)

#ifdef __cplusplus
}
#endif

#endif /* BMS_ACTION_H */
