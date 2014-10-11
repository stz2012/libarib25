#ifndef ARIB25API_H
#define ARIB25API_H 1 

/* If building or using arib25 as a DLL, define ARIB25_DLL.
 *  */
/* TODO: define ARIB25_BUILD_DLL when building this library as DLL.
 *  */
#if defined _WIN32 || defined __CYGWIN__
  #ifdef ARIB25_DLL
    #ifdef ARIB25_BUILD_DLL
      #ifdef __GNUC__
        #define ARIB25API __attribute__ ((dllexport))
      #else
        #define ARIB25API extern __declspec(dllexport)
      #endif
    #else
      #ifdef __GNUC__
        #define ARIB25API __attribute__ ((dllimport))
      #else
        #define ARIB25API extern __declspec(dllimport)
      #endif
    #endif
  #else
    #if __GNUC__ >= 4
      #define ARIB25API __attribute__ ((visibility ("default")))
    #else
      #define ARIB25API extern
    #endif
  #endif
  #define DLL_LOCAL
#else
  #if __GNUC__ >= 4
    #define ARIB25API __attribute__ ((visibility ("default")))
  #else
    #define ARIB25API extern
  #endif
#endif

#endif
