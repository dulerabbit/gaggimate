from pinout.core import Group, Image
from pinout.components.layout import Diagram
from pinout.components.pinlabel import PinLabelGroup, PinLabel, Body
from pinout.components.text import TextBlock
from pinout.components.annotation import AnnotationLabel
from pinout.components import leaderline as lline
from pinout.components.legend import Legend


# Import data for the diagram
import data

# Create a new diagram
diagram = Diagram(1600, 1100, "diagram")

# Add a stylesheet
diagram.add_stylesheet("styles.css", True)

# Create a group to hold the pinout-diagram components.
graphic = diagram.add(Group(169, 42))

# Add and embed an image
hardware = graphic.add(Image("../docs/assets/pcb_top.png", embed=True))

# Measure and record key locations with the hardware Image instance
hardware.add_coord("ac_in", 1015, 65)
hardware.add_coord("relais_out", 1015, 205)
hardware.add_coord("screen", 445, 506)
hardware.add_coord("ssr", 445, 403)
hardware.add_coord("uart", 275, 403)
hardware.add_coord("temp_sensor", 133, 198)
hardware.add_coord("i2c", 113, 320)
hardware.add_coord("buttons", 113, 403)
hardware.add_coord("ext", 97, 470)
# Other (x,y) pairs can also be stored here
hardware.add_coord("pin_pitch_v", 0, 30)
hardware.add_coord("pin_pitch_v_35", 0, 40)
hardware.add_coord("pin_pitch_v_ac", 0, 60)
hardware.add_coord("pin_pitch_h", 30, 0)


graphic.add(
    PinLabelGroup(
        x=hardware.coord("uart").x,
        y=hardware.coord("uart").y,
        scale=(-1, -1),
        pin_pitch=hardware.coord("pin_pitch_h", raw=True),
        label_start=(222, 280),
        label_pitch=hardware.coord("pin_pitch_v", raw=True),
        labels=data.uart,
        leaderline=lline.Curved(direction="vh"),
    )
)

graphic.add(
    PinLabelGroup(
        x=hardware.coord("temp_sensor").x,
        y=hardware.coord("temp_sensor").y,
        scale=(-1, 1),
        pin_pitch=hardware.coord("pin_pitch_v_35", raw=True),
        label_start=(80, -40),
        label_pitch=hardware.coord("pin_pitch_v", raw=True),
        labels=data.temp_sensor,
        leaderline=lline.Curved(),
    )
)

graphic.add(
    PinLabelGroup(
        x=hardware.coord("buttons").x,
        y=hardware.coord("buttons").y,
        scale=(-1, -1),
        pin_pitch=hardware.coord("pin_pitch_h", raw=True),
        label_start=(60,-60),
        label_pitch=hardware.coord("pin_pitch_v", raw=True),
        labels=data.buttons,
        leaderline=lline.Straight(),
    )
)

graphic.add(
    PinLabelGroup(
        x=hardware.coord("i2c").x,
        y=hardware.coord("i2c").y,
        scale=(-1, -1),
        pin_pitch=hardware.coord("pin_pitch_h", raw=True),
        label_start=(60, -20),
        label_pitch=hardware.coord("pin_pitch_v", raw=True),
        labels=data.i2c,
        leaderline=lline.Straight(),
    )
)

graphic.add(
    PinLabelGroup(
        x=hardware.coord("ext").x,
        y=hardware.coord("ext").y,
        scale=(-1, 1),
        pin_pitch=hardware.coord("pin_pitch_h", raw=True),
        label_start=(44, 20),
        label_pitch=hardware.coord("pin_pitch_v", raw=True),
        labels=data.ext,
        leaderline=lline.Curved(direction="vh"),
    )
)

graphic.add(
    PinLabelGroup(
        x=hardware.coord("ssr").x,
        y=hardware.coord("ssr").y,
        scale=(1, 1),
        pin_pitch=hardware.coord("pin_pitch_v", raw=True),
        label_start=(630, 0),
        label_pitch=hardware.coord("pin_pitch_v", raw=True),
        labels=data.ssr,
    )
)

graphic.add(
    PinLabelGroup(
        x=hardware.coord("screen").x,
        y=hardware.coord("screen").y,
        scale=(1, 1),
        pin_pitch=hardware.coord("pin_pitch_v", raw=True),
        label_start=(630, 0),
        label_pitch=hardware.coord("pin_pitch_v", raw=True),
        labels=data.screen,
    )
)

graphic.add(
    PinLabelGroup(
        x=hardware.coord("ac_in").x,
        y=hardware.coord("ac_in").y,
        scale=(1, 1),
        pin_pitch=hardware.coord("pin_pitch_v_ac", raw=True),
        label_start=(60, 0),
        label_pitch=hardware.coord("pin_pitch_v_ac", raw=True),
        labels=data.ac_in,
    )
)
graphic.add(
    PinLabelGroup(
        x=hardware.coord("relais_out").x,
        y=hardware.coord("relais_out").y,
        scale=(1, 1),
        pin_pitch=hardware.coord("pin_pitch_v_ac", raw=True),
        label_start=(60, 0),
        label_pitch=hardware.coord("pin_pitch_v_ac", raw=True),
        labels=data.relais_out,
    )
)

graphic.add(
    AnnotationLabel(
        x=410,
        y=760,
        scale=(-1, 1),
        content={"x": 102, "y": 76, "content": data.annotation_usb},
        body={"y": 58, "width": 125},
        target={"x": -20, "y": -20, "width": 40, "height": 40, "corner_radius": 20},
    )
)
graphic.add(
    AnnotationLabel(
        x=515,
        y=750,
        scale=(-1, 1),
        content={"x": 204, "y": 146, "content": data.annotation_btn_reset},
        body={"x": 142, "y": 128, "width": 125},
        target={"x": -20, "y": -20, "width": 40, "height": 40, "corner_radius": 20},
    )
)
graphic.add(
    AnnotationLabel(
        x=587,
        y=750,
        scale=(-1, 1),
        content={"x": 279, "y": 206, "content": data.annotation_btn_boot},
        body={"x": 217, "y": 188, "width": 125},
        target={"x": -20, "y": -20, "width": 40, "height": 40, "corner_radius": 20},
    )
)
