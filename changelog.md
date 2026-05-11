## Modifications made while refactoring code:
- Modified the Return codes of function 'safeConcatPath'. Returns 0 now upon function success as per Standard return code API.
- Removed multiple return statements from 'safeConcatPath' to add MISRA-C functional Safety compliance.
- Replaced index hard coding at various locations. e.g. used 'CONTENT_LEN' instead of '32', etc.
- Added default 'else' branch to 'pinmode','write' command.
- Added analogWrite functionality to 'write' command.
- Added analogRead functionality to 'read' command.