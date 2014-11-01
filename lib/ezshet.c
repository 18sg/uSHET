#include "ezshet.h"

void _ezshet_set_is_registered(shet_state_t *shet, shet_json_t json, void *_is_registered) {
	USE(shet);
	USE(json);
	bool *is_registered = (bool *)_is_registered;
	*is_registered = true;
}

void _ezshet_clear_is_registered(shet_state_t *shet, shet_json_t json, void *_is_registered) {
	USE(shet);
	USE(json);
	bool *is_registered = (bool *)_is_registered;
	*is_registered = false;
}

void _ezshet_inc_error_count(shet_state_t *shet, shet_json_t json, void *_error_count) {
	USE(shet);
	USE(json);
	unsigned int *error_count = (unsigned int *)_error_count;
	(*error_count)++;
}
