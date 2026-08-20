import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    ICON_ROTATE_RIGHT,
    STATE_CLASS_MEASUREMENT_ANGLE,
    UNIT_DEGREES,
)

from .. import TMAG5273Component, tmag5273_ns

CODEOWNERS = ["@khbenjamin"]
DEPENDENCIES = ["tmag5273"]

TMAG5273Sensor = tmag5273_ns.class_(
    "TMAG5273Sensor",
    sensor.Sensor,
    cg.PollingComponent,
    cg.Parented.template(TMAG5273Component),
)

CONF_TMAG5273_ID = "tmag5273_id"


CONFIG_SCHEMA = (
    sensor.sensor_schema(
        TMAG5273Sensor,
        unit_of_measurement=UNIT_DEGREES,
        accuracy_decimals=2,
        icon=ICON_ROTATE_RIGHT,
        state_class=STATE_CLASS_MEASUREMENT_ANGLE,
    )
    .extend(
        {
            cv.GenerateID(CONF_TMAG5273_ID): cv.use_id(TMAG5273Component),
        }
    )
    .extend(cv.polling_component_schema("60s"))
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_TMAG5273_ID])
