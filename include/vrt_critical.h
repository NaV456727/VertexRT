#ifndef VRT_CRITICAL_H
#define VRT_CRITICAL_H

#ifdef __cplusplus
extern "C"
{
#endif

    void vrt_kernel_critical_enter(void);
    void vrt_kernel_critical_exit(void);

    void vrt_kernel_critical_enter_isr(void);
    void vrt_kernel_critical_exit_isr(void);

#ifdef __cplusplus
}
#endif

#endif /* VRT_CRITICAL_H */