idf_component_register(SRCS "CJsonConvertor.cpp" "COTASystem.cpp" "CDateTimeSystem.cpp" "CNvsSystem.cpp" "CSpiffsSystem.cpp" 
                    "CBufferSystem.cpp"
                    "CJsonReadStream.cpp"
                    "CDateTimeSystem.cpp"
                    "CCRC8.cpp"
                    "CCRC16.cpp"
                    "CJsonConvertor.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES spiffs nvs_flash app_update nlohmann-json)
