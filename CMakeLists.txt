
set(TinyGSM_SRCS 
    src/TinyGsmModem.tpp
#    src/TinyGsmTCP.tpp
    )
set(TinyGSM_INCL ${CMAKE_CURRENT_SOURCE_DIR}/src)

idf_component_register(
    INCLUDE_DIRS ${TinyGSM_INCL} 
    SRCS ${TinyGSM_SRCS}
    REQUIRES 
        arduino 
        ms_Logger
        ms_General
    )
