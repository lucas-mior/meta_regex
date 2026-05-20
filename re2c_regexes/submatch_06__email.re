char      = [a-z0-9!#$%&'*+/=?^_`{|}~-];
before_at = char+ ([.] char+)*;
az09      = [a-z0-9];
az09dash  = [a-z0-9-]* [a-z0-9];
after_at  = (az09 az09dash? [.])+ az09 az09dash?;
email     = before_at [@] @p after_at [\n];
skip      = [^\n\x00]* [\n];
