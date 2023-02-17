// Decode decodes an array of bytes into an object.
//  - fPort contains the LoRaWAN fPort number
//  - bytes is an array of bytes, e.g. [225, 230, 255, 0]
//  - variables contains the device variables e.g. {"calibration": "3.5"} (both the key / value are of type string)
// The function must return an object, e.g. {"temperature": 22.5}

function Decode_u8(bytes, pos)
{
    return bytes[pos];
}


function Decode_u16(bytes, pos)
{
    return (bytes[pos + 1] << 8) | bytes[pos];
}


function Decode_u32(bytes, pos)
{
    return (Decode_u16(bytes, pos + 2) << 16) | Decode_u16(bytes, pos);
}


function Decode_u64(bytes, pos)
{
    return (Decode_u32(bytes, pos + 4) << 32 | Decode_u32(bytes, pos));
}


function Decode_i8(bytes, pos)
{
    var UINT8_MAX = 256;
    var val;
    pos, val = Decode_u8(bytes, pos);
    if (val > UINT8_MAX / 2)
    {
        val -= UINT8_MAX;
    }
    return (pos, val);
}


function Decode_i16(bytes, pos)
{
    var UINT16_MAX = 0xFFFF;
    var val;
    pos, val = Decode_u16(bytes, pos);
    if (val & 0x8000)
    {
        val -= (UINT16_MAX + 1);
    }
    return (pos, val);
}


function Decode_i32(bytes, pos)
{
    var UINT32_MAX = 0xFFFFFFFF;
    var val;
    pos, val = Decode_u32(bytes, pos);
    if (val > UINT32_MAX / 2)
    {
        val -= (UINT32_MAX + 1);
    }
    return (pos, val);
}


function Decode_i64(bytes, pos)
{
    var UINT64_MAX = 0xFFFFFFFFFFFFFFFF;
    var val;
    pos, val = Decode_u64(bytes, pos);
    if (val > UINT64_MAX / 2)
    {
        val -= (UINT64_MAX - 1);
    }
    return (pos, val);
}


function Decode_float(bytes, pos)
{
    return Decode_i32(bytes, pos) / 1000;
}


function Decode_double(bytes, pos)
{
    return Decode_i64(bytes, pos) / 1000;
}


function Decode_string(bytes, pos)
{
    var val = "";
    for (var i = 0; i < 8; i++)
    {
        val += String.fromCharCode(bytes[pos+i]);
    }
    return val;
}


function Decode_value(value_type, bytes, pos)
{
    switch (value_type)
    {
        case 0x01:
            return Decode_u8(bytes, pos);
        case 0x02:
            return Decode_u16(bytes, pos);
        case 0x03:
            return Decode_u32(bytes, pos);
        case 0x04:
            return Decode_u64(bytes, pos);
        case 0x11:
            return Decode_i8(bytes, pos);
        case 0x12:
            return Decode_i16(bytes, pos);
        case 0x13:
            return Decode_i32(bytes, pos);
        case 0x14:
            return Decode_i64(bytes, pos);
        case 0x15:
            return Decode_float(bytes, pos);
        case 0x16:
            return Decode_double(bytes, pos);
        case 0x20:
            return Decode_string(bytes, pos);
        default:
            return false;
    }
}


function Value_sizes(value_type)
{
    switch (value_type)
    {
        case 0x01:
            return 1;
        case 0x02:
            return 2;
        case 0x03:
            return 4;
        case 0x04:
            return 8;
        case 0x11:
            return 1;
        case 0x12:
            return 2;
        case 0x13:
            return 4;
        case 0x14:
            return 8;
        case 0x15:
            return 4;
        case 0x16:
            return 8;
        case 0x20:
            return 8;
        default:
            return false;
    }
}


function Error_lookup(err)
{
    switch (err)
    {
        case 0x00:
            return "No response"
        case 0x01:
            return "Success";
        case 0x02:
            return "Error"
        default:
            break;
    }
    var resp = 'Unknown error: ' + err;
    return resp;
}


function Decode(fPort, bytes, variables)
{
    var pos = 0;
    var obj = {};

    var protocol_version = bytes[pos++];

    if (protocol_version != 1 && protocol_version != 2)
    {
        return obj;
    }

    var name;
    while(pos < bytes.length)
    {
        name = "";
        for (var i = 0; i < 4; i++)
        {
            if (bytes[pos] != 0)
            {
                name += String.fromCharCode(bytes[pos]);
            }
            pos++;
        }
        name = name.trim();
        name = name.replace(" ", "_");
        var data_type = bytes[pos++];
        var value_type, func;
        var mean, min, max;
        switch (data_type)
        {
            // Single measurement
            case 1:
                value_type = bytes[pos++];
                mean = Decode_value(value_type, bytes, pos);
                pos += Value_sizes(value_type);
                if (name == "ERR")
                    mean = Error_lookup(mean);
                obj[name] = mean;
                break;
            // Multiple measurement
            case 2:
                value_type = bytes[pos++];
                mean = Decode_value(value_type, bytes, pos);
                pos += Value_sizes(value_type);
                obj[name] = mean;

                value_type = bytes[pos++];
                min = Decode_value(value_type, bytes, pos);
                pos += Value_sizes(value_type);
                obj[name+"_min"] = min;

                value_type = bytes[pos++];
                max = Decode_value(value_type, bytes, pos);
                pos += Value_sizes(value_type);
                obj[name+"_max"] = max;
                break;
            default:
                return obj;
        }
    }
    return obj;
}

// Encode encodes the given object into an array of bytes.
//  - fPort contains the LoRaWAN fPort number
//  - obj is an object, e.g. {"temperature": 22.5}
//  - variables contains the device variables e.g. {"calibration": "3.5"} (both the key / value are of type string)
// The function must return an array of bytes, e.g. [225, 230, 255, 0]
function Encode(fPort, obj, variables)
{
    var MAX_LENGTH = 56;
    var topics = Object.keys(obj);
    var bytes = [];

    if (topics.length > 1)
    {
        return bytes;
    }

    var name = topics[0];
    var payload = obj[name];

    if (name.length > 4)
    {
        return bytes;
    }

    // Protocol version
    bytes.push(1);

    // Send name
    for (var i = 0; i < 4; i++)
    {
        if (name.length > i)
        {
            bytes.push(name.charCodeAt(i));
        }
        else
        {
            bytes.push(0);
        }
    }

    // Send Payload
    if (typeof payload === "string")
    {
        // 1 For protocol version, 4 for name length, X for payload length
        if (1 + 4 + payload.length > MAX_LENGTH)
        {
            return [];
        }
        for (var i = 0; i < payload.length; i++)
        {
            bytes.push(payload.charCodeAt(i));
        }
    }
    else if (typeof payload === "number" || typeof payload === "bigint")
    {
        var n = 0
        do
        {
            bytes.push( (payload >> n) & 0xFF)
            n += 8
        }
        while ((1 << n) <= payload)
    }

    return bytes;
}

module.exports = { Decode, Encode }
