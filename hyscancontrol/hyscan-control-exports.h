#ifndef __HYSCAN_CONTROL_EXPORTS_H__
#define __HYSCAN_CONTROL_EXPORTS_H__

#if defined (_WIN32)
  #if defined (hyscancontrol_EXPORTS)
    #define HYSCAN_CONTROL_EXPORT __declspec (dllexport)
  #else
    #define HYSCAN_CONTROL_EXPORT __declspec (dllimport)
  #endif
#else
  #define HYSCAN_CONTROL_EXPORT
#endif

#endif /* __HYSCAN_CONTROL_EXPORTS_H__ */
