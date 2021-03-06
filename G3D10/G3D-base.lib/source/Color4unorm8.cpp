/**
  \file G3D-base.lib/source/Color4unorm8.cpp

  G3D Innovation Engine http://casual-effects.com/g3d
  Copyright 2000-2019, Morgan McGuire
  All rights reserved
  Available under the BSD License
*/
#include "G3D-base/platform.h"
#include "G3D-base/g3dmath.h"
#include "G3D-base/Color4unorm8.h"
#include "G3D-base/Color4.h"
#include "G3D-base/BinaryInput.h"
#include "G3D-base/BinaryOutput.h"

namespace G3D {

Color4unorm8::Color4unorm8(const class Color4& c) : r(c.r), g(c.g), b(c.b), a(c.a) {
}


Color4unorm8::Color4unorm8(class BinaryInput& bi) {
    deserialize(bi);
}


void Color4unorm8::serialize(class BinaryOutput& bo) const {
    bo.writeUInt8(r.bits());
    bo.writeUInt8(g.bits());
    bo.writeUInt8(b.bits());
    bo.writeUInt8(a.bits());
}


void Color4unorm8::deserialize(class BinaryInput& bi) {
    r = unorm8::fromBits(bi.readUInt8());
    g = unorm8::fromBits(bi.readUInt8());
    b = unorm8::fromBits(bi.readUInt8());
    a = unorm8::fromBits(bi.readUInt8());
}


}
