#include "ezshet.h"

void _ezshet_set_is_registered(shet_state_t *shet, shet_json_t json, void *_is_registered) {
	bool *is_registered = (bool *)_is_registered;
	*is_registered = true;
}

void _ezshet_clear_is_registered(shet_state_t *shet, shet_json_t json, void *_is_registered) {
	bool *is_registered = (bool *)_is_registered;
	*is_registered = false;
}
