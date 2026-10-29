#ifndef MOCK_PROVIDER_H
#define MOCK_PROVIDER_H

#include <stdint.h>

// MockProvider — the demo data provider. Animates BikeState so the whole UI
// can be developed and demoed on the bench with no GPS / phone / headset.
// Call mock_provider_update() regularly from loop(); it self-throttles to
// 100 ms. While a live BLE nav session is fresh it leaves nav fields alone
// (see bike_state_nav_live). The bike_control_*() UI actions are also
// implemented here — real drivers will take them over later.
void mock_provider_init(void);
void mock_provider_update(uint32_t nowMs);

#endif // MOCK_PROVIDER_H
