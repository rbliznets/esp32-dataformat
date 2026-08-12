idf_component_register(SRCS "CFileSignature.cpp" "CLittlefsSystem.cpp" "CJsonConvertor.cpp" "COTASystem.cpp" "CDateTimeSystem.cpp" "CNvsSystem.cpp" "CSpiffsSystem.cpp" 
                    "CBufferSystem.cpp"
                    "CJsonReadStream.cpp"
                    "CDateTimeSystem.cpp"
                    "CCRC8.cpp"
                    "CCRC16.cpp"
                    "CJsonConvertor.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES spiffs nvs_flash app_update nlohmann-json mbedtls esp_timer)
target_compile_options(${COMPONENT_LIB} PRIVATE -Os)
# CRC8/CRC16 are on the per-packet hot path - keep them at the project's global optimization level.
set_source_files_properties(CCRC8.cpp CCRC16.cpp PROPERTIES COMPILE_OPTIONS -O2)
