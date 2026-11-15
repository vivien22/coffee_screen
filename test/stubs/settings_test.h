#pragma once
// Set one field of the saved settings: SAVE(store, flush, 8)
#define SAVE(store, field, value) do { Settings _t = (store).load(); _t.field = (value); (store).save(_t); } while (0)
