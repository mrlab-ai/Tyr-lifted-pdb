import re


TYPE_TOKEN_PATTERN = re.compile(r"\s*([A-Za-z_][A-Za-z0-9_]*|\[|\]|,)")


def parse_attribute_type(type_expr, attribute_type_cls):
    if not isinstance(type_expr, str) or not type_expr:
        raise ValueError("attribute type must be a non-empty string.")

    tokens = TYPE_TOKEN_PATTERN.findall(type_expr)
    if "".join(tokens) != re.sub(r"\s+", "", type_expr):
        raise ValueError("unexpected trailing input.")

    def parse_type(position):
        if position >= len(tokens) or tokens[position] in {"[", "]", ","}:
            raise ValueError("expected type name.")

        attribute_type = attribute_type_cls(tokens[position])
        position += 1
        parsed_arguments = []

        if position < len(tokens) and tokens[position] == "[":
            position += 1
            while True:
                argument, position = parse_type(position)
                parsed_arguments.append(argument)
                if position >= len(tokens):
                    raise ValueError("unclosed '['.")
                if tokens[position] == "]":
                    position += 1
                    break
                if tokens[position] != ",":
                    raise ValueError("expected ',' or ']'.")
                position += 1

        return {"type": attribute_type.value, "args": parsed_arguments}, position

    parsed_type, position = parse_type(0)
    if position != len(tokens):
        raise ValueError("unexpected trailing input.")
    return parsed_type
