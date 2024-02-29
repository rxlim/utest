
#if defined _WIN32 || defined __CYGWIN__
  #ifdef BUILD_UNIT_@{BNAME}
    #ifdef __GNUC__
      #define API_@{BNAME} __attribute__ ((dllexport))
    #else
      #define API_@{BNAME} __declspec(dllexport)
    #endif
  #else
    #ifdef __GNUC__
      #define API_@{BNAME} __attribute__ ((dllimport))
    #else
      #define API_@{BNAME} __declspec(dllimport)
    #endif
  #endif
  #define HIDDEN
#else
  #define API_@{BNAME} __attribute__ ((visibility ("default")))
  #define HIDDEN __attribute__ ((visibility ("hidden")))
#endif
