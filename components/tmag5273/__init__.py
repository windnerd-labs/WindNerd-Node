import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@khbenjamin"]
DEPENDENCIES = ["i2c"]
MULTI_CONF = True

tmag5273_ns = cg.esphome_ns.namespace("tmag5273")
TMAG5273Component = tmag5273_ns.class_("TMAG5273Component", cg.Component, i2c.I2CDevice)

CONF_AXIS = "axis"

# ANGLE_EN pair the device computes the angle from. Only one pair can be active
# at a time (a device-level register), so it lives on the hub, not the sensor.
AngleEn = tmag5273_ns.enum("AngleEn")
ANGLE_AXIS = {
    "XY": AngleEn.ANGLE_EN_XY,
    "YZ": AngleEn.ANGLE_EN_YZ,
    "XZ": AngleEn.ANGLE_EN_XZ,
}


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(TMAG5273Component),
            cv.Required(CONF_AXIS): cv.enum(ANGLE_AXIS, upper=True, space="_"),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    # 0x00 = auto-detect: setup() probes the factory addresses of the four
    # orderable variants (A=0x35, B=0x22, C=0x78, D=0x44) and uses the first
    # chip that answers with TI's manufacturer ID.
    .extend(i2c.i2c_device_schema(0x00))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_angle_en(config[CONF_AXIS]))
