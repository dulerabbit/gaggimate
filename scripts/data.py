legend = [
    ("Ground", "gnd"),
    ("Power", "pwr"),
    ("PWM", "pwm"),
    ("AC", "ac"),
    ("Doc", "doc")
]

# Pinlabels

temp_sensor = [
    [
        ("T-", "temp"),
    ],
    [
        ("T+", "temp"),
    ],
]

uart = [
    [
        ("+5V", "pwr"),
    ],
    [
        ("TX", "comms"),
        ("gpio43", "doc"),
    ],
    [
        ("RX", "comms"),
        ("gpio44", "doc"),
    ],
    [
        ("GND", "gnd"),
    ],
]

i2c = [
    [
        ("+5V", "pwr"),
    ],
    [
        ("SCL", "comms"),
        ("gpio17", "doc"),
    ],
    [
        ("SDA", "comms"),
        ("gpio18", "doc"),
    ],
    [
        ("SDA2", "comms"),
        ("gpio39", "doc"),
    ],
    [
        ("GND", "gnd"),
    ],
]

buttons = [
    [
        ("Brew", "gpio"),
        ("gpio38", "doc"),
    ],
    [
        ("Steam", "gpio"),
        ("gpio48", "doc"),
    ],
    [
        ("GND", "gnd"),
    ],
    [
        ("GND", "gnd"),
    ],
]

ext = [
    [
        ("GND", "gnd"),
    ],
    [
        ("GND", "gnd"),
    ],
    [
        ("gpio1", "doc"),
    ],
    [
        ("gpio2", "doc"),
    ],
    [
        ("gpio8", "doc"),
    ],
    [
        ("gpio12", "doc"),
    ],
    [
        ("gpio13", "doc"),
    ],
    [
        ("+3.3V", "pwr"),
    ],
    [
        ("+5V", "pwr"),
    ],
    [
        ("+5V", "pwr"),
    ],
]

screen = [
    [
        ("GND", "gnd"),
        ("Screen", "doc"),
    ],
    [
        ("+3.3V", "pwr"),
        ("Screen", "doc"),
    ],
]

ssr = [
    [
        ("GND", "gnd"),
    ],
    [
        ("SSR", "pwm"),
        ("gpio14", "doc"),
    ],
]

ac_in = [
    [
        ("N", "ac"),
    ],
    [
        ("L", "ac"),
    ],
]

relais_out = [
    [
        ("Grinder", "ac"),
        ("gpio11", "doc"),
    ],
    [
        ("Valve", "ac"),
        ("gpio10", "doc"),
    ],
    [
        ("Pump", "ac"),
        ("gpio9", "doc"),
    ],
]

annotation_usb = ["USB-C", "port"]
annotation_btn_boot = ["BOOT", "button"]
annotation_btn_reset = ["EN", "button"]
