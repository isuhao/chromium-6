// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_tiles/ntp_tile.h"

namespace ntp_tiles {

NTPTile::NTPTile() : source(NTPTileSource::TOP_SITES) {}

NTPTile::~NTPTile() {}

NTPTile::NTPTile(NTPTile&&) = default;

NTPTile& NTPTile::operator=(NTPTile&&) = default;

}  // namespace ntp_tiles
