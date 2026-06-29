idf_component_register(SRCS "motion_control.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES driver stepper hx711 esp_driver_gptimer main)
