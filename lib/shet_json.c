#include "shet.h"
#include "shet_json.h"

shet_json_t shet_next_token(shet_json_t json) {
	shet_json_t next = json;
	
	switch (json.token->type) {
		case JSMN_PRIMITIVE:
		case JSMN_STRING:
			next.token++;
			return next;
		
		case JSMN_ARRAY:
		case JSMN_OBJECT:
			next.token++;
			for (int i = 0; i < json.token->size; i++)
				next = shet_next_token(next);
			return next;
		
		default:
			return json;
	}
}
