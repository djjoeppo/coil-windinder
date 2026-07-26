idf_component_register(SRCS "motion_control.c"
                    INCLUDE_DIRS "include" "${project_dir}/include" "${CMAKE_SOURCE_DIR}/include"
                    PRIV_REQUIRES driver stepper hx711 esp_driver_gptimer esp_timer)