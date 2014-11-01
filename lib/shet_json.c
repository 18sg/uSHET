#include "shet.h"
#include "shet_json.h"

#ifdef __cplusplus
extern "C" {
#endif

shet_json_t shet_next_token(shet_json_t json) {
	shet_json_t next = json;
	
	switch (json.token->type) {
		case JSMN_PRIMITIVE:
		case JSMN_STRING:
			next.token++;
			return next;
		
		case JSMN_ARRAY:
		case JSMN_OBJECT: {
			next.token++;
			int i;
			for (i = 0; i < json.token->size; i++)
				next = shet_next_token(next);
			return next;
		}
		
		default:
			return json;
	}
}


unsigned int shet_count_tokens(shet_json_t json) {
	switch (json.token->type) {
		case JSMN_PRIMITIVE:
		case JSMN_STRING:
			return 1;
		
		case JSMN_ARRAY:
		case JSMN_OBJECT:
			return shet_next_token(json).token - json.token;
		
		default:
			return 0;
	}
}

#ifdef __cplusplus
}
#endif
