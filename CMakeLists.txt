

message(INFO "<MS>")
message(INFO "<MS> TinyGSM >>")
message(INFO "<MS>")
message(INFO "<MS> CMAKE_CURRENT_SOURCE_DIR: ${CMAKE_CURRENT_SOURCE_DIR}")


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
    )

message(INFO "<MS>")
message(INFO "<MS> TinyGSM <<")
message(INFO "<MS>")
