#ifndef __CAR_POSE_H__
#define __CAR_POSE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "sensor_ops.h"
#include "sensor_wit.h"

extern const volatile WIT_Data_t *carpose_imu;
extern const volatile OPS_Pose_t *carpose_ops;

void CarPose_Init(void);

#ifdef __cplusplus
}
#endif

#endif
