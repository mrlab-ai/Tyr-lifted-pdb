from enum import Enum
import json

from attribute_parser import parse_attribute_type as parse_attribute_type_expr


class AttributeType(Enum):
    BOOL = "bool"
    FLOAT = "float"
    INT = "int"
    STR = "str"
    LIST = "list"
    SET = "set"
    DICT = "dict"


class AttributeCompare(Enum):
    STRICT_EQUALITY = "strict_equality"
    LOWER_IS_BETTER = "lower_is_better"
    HIGHER_IS_BETTER = "higher_is_better"


def parse_attribute_type(type_expr):
    parsed_type = parse_attribute_type_expr(type_expr, AttributeType)
    validate_parsed_attribute_type(parsed_type)
    return parsed_type


def validate_parsed_attribute_type(parsed_type):
    attribute_type = AttributeType(parsed_type["type"])
    parsed_arguments = parsed_type["args"]

    if attribute_type in {AttributeType.LIST, AttributeType.SET}:
        if len(parsed_arguments) != 1:
            raise ValueError(f"{attribute_type.value} expects exactly one type argument.")
    elif attribute_type == AttributeType.DICT:
        if len(parsed_arguments) not in {1, 2}:
            raise ValueError("dict expects one value type or key and value type arguments.")
        if len(parsed_arguments) == 2 and parsed_arguments[0]["type"] in {AttributeType.LIST.value, AttributeType.SET.value, AttributeType.DICT.value}:
            raise ValueError("dict key type must be scalar.")
    elif parsed_arguments:
        raise ValueError(f"{attribute_type.value} does not accept type arguments.")

    for argument in parsed_arguments:
        validate_parsed_attribute_type(argument)


def require_mapping(value, name):
    if not isinstance(value, dict):
        raise ValueError(f"'{name}' must be an object.")
    return value


def require_non_empty_string(value, name):
    if not isinstance(value, str) or not value:
        raise ValueError(f"'{name}' must be a non-empty string.")
    return value


def validate_attributes(attributes):
    require_mapping(attributes, "attributes")

    for name, config in attributes.items():
        require_non_empty_string(name, "attribute name")
        require_mapping(config, f"attributes.{name}")

        try:
            parse_attribute_type(config["type"])
        except KeyError as error:
            raise ValueError(f"Attribute '{name}' is missing required field 'type'.") from error
        except ValueError as error:
            valid_values = ", ".join(attribute_type.value for attribute_type in AttributeType)
            raise ValueError(
                f"Attribute '{name}' has invalid type '{config['type']}'. Expected one of: {valid_values}, "
                "or a composite type such as list[int], set[int], or dict[str, int]."
            ) from error

        try:
            AttributeCompare(config["compare"])
        except KeyError as error:
            raise ValueError(f"Attribute '{name}' is missing required field 'compare'.") from error
        except ValueError as error:
            valid_values = ", ".join(attribute_compare.value for attribute_compare in AttributeCompare)
            raise ValueError(f"Attribute '{name}' has invalid compare '{config['compare']}'. Expected one of: {valid_values}.") from error


def validate_suite(suite):
    require_mapping(suite, "suite")

    try:
        domains = require_mapping(suite["domains"], "domains")
    except KeyError as error:
        raise ValueError("Suite is missing required field 'domains'.") from error

    if not domains:
        raise ValueError("'domains' must contain at least one domain.")

    validate_attributes(suite.get("attributes", {}))

    for domain_name, domain_config in domains.items():
        require_non_empty_string(domain_name, "domain name")
        require_mapping(domain_config, f"domains.{domain_name}")

        try:
            require_non_empty_string(domain_config["domain_file"], f"domains.{domain_name}.domain_file")
        except KeyError as error:
            raise ValueError(f"Domain '{domain_name}' is missing required field 'domain_file'.") from error

        try:
            tasks = require_mapping(domain_config["tasks"], f"domains.{domain_name}.tasks")
        except KeyError as error:
            raise ValueError(f"Domain '{domain_name}' is missing required field 'tasks'.") from error

        if not tasks:
            raise ValueError(f"Domain '{domain_name}' must contain at least one task.")

        for task_name, task_file in tasks.items():
            require_non_empty_string(task_name, f"domains.{domain_name} task name")
            require_non_empty_string(task_file, f"domains.{domain_name}.tasks.{task_name}")


def canonical_json_value(value):
    return json.dumps(to_json_comparable(value), sort_keys=True, separators=(",", ":"))


def to_json_comparable(value):
    if isinstance(value, frozenset):
        return sorted(value)
    if isinstance(value, list):
        return [to_json_comparable(item) for item in value]
    if isinstance(value, dict):
        return {str(key): to_json_comparable(item) for key, item in value.items()}
    return value


def parse_dict_key(attribute_name, key_type, key):
    attribute_type = AttributeType(key_type["type"])

    if attribute_type == AttributeType.STR:
        return key

    if attribute_type == AttributeType.INT:
        try:
            parsed_key = int(key)
        except ValueError as error:
            raise ValueError(f"Attribute '{attribute_name}' expected int dict key, got {key!r}.") from error
        if str(parsed_key) != key:
            raise ValueError(f"Attribute '{attribute_name}' expected canonical int dict key, got {key!r}.")
        return parsed_key

    if attribute_type == AttributeType.FLOAT:
        try:
            return float(key)
        except ValueError as error:
            raise ValueError(f"Attribute '{attribute_name}' expected float dict key, got {key!r}.") from error

    if attribute_type == AttributeType.BOOL:
        if key == "true":
            return True
        if key == "false":
            return False
        raise ValueError(f"Attribute '{attribute_name}' expected bool dict key 'true' or 'false', got {key!r}.")

    raise ValueError(f"Attribute '{attribute_name}' expected scalar dict key type, got {attribute_type.value}.")


def validate_typed_value(attribute_name, type_config, value):
    attribute_type = AttributeType(type_config["type"])

    if attribute_type == AttributeType.BOOL:
        if isinstance(value, bool):
            return
        raise ValueError(f"Attribute '{attribute_name}' expected bool value, got {type(value).__name__}.")

    if attribute_type == AttributeType.FLOAT:
        if isinstance(value, (int, float)) and not isinstance(value, bool):
            return
        raise ValueError(f"Attribute '{attribute_name}' expected float value, got {type(value).__name__}.")

    if attribute_type == AttributeType.INT:
        if isinstance(value, int) and not isinstance(value, bool):
            return
        if isinstance(value, float) and value.is_integer():
            return
        raise ValueError(f"Attribute '{attribute_name}' expected int value, got {type(value).__name__}.")

    if attribute_type == AttributeType.STR:
        if isinstance(value, str):
            return
        raise ValueError(f"Attribute '{attribute_name}' expected str value, got {type(value).__name__}.")

    if attribute_type == AttributeType.LIST:
        if not isinstance(value, list):
            raise ValueError(f"Attribute '{attribute_name}' expected list value, got {type(value).__name__}.")
        item_type = type_config["args"][0]
        for index, item in enumerate(value):
            validate_typed_value(f"{attribute_name}[{index}]", item_type, item)
        return

    if attribute_type == AttributeType.SET:
        if not isinstance(value, list):
            raise ValueError(f"Attribute '{attribute_name}' expected set value encoded as list, got {type(value).__name__}.")
        item_type = type_config["args"][0]
        seen_items = set()
        for index, item in enumerate(value):
            validate_typed_value(f"{attribute_name}[{index}]", item_type, item)
            canonical_item = canonical_json_value(normalize_typed_value(item_type, item))
            if canonical_item in seen_items:
                raise ValueError(f"Attribute '{attribute_name}' has duplicate set item at index {index}.")
            seen_items.add(canonical_item)
        return

    if attribute_type == AttributeType.DICT:
        if not isinstance(value, dict):
            raise ValueError(f"Attribute '{attribute_name}' expected dict value, got {type(value).__name__}.")
        key_type = type_config["args"][0] if len(type_config["args"]) == 2 else None
        value_type = type_config["args"][-1]
        seen_keys = set()
        for key, item in value.items():
            if not isinstance(key, str):
                raise ValueError(f"Attribute '{attribute_name}' expected str dict key, got {type(key).__name__}.")
            if key_type is not None:
                parsed_key = parse_dict_key(attribute_name, key_type, key)
                canonical_key = canonical_json_value(parsed_key)
                if canonical_key in seen_keys:
                    raise ValueError(f"Attribute '{attribute_name}' has duplicate semantic dict key {key!r}.")
                seen_keys.add(canonical_key)
            validate_typed_value(f"{attribute_name}.{key}", value_type, item)
        return


def validate_attribute_value(attribute_name, attribute_config, value):
    if value is None:
        return

    validate_typed_value(attribute_name, parse_attribute_type(attribute_config["type"]), value)


def normalize_typed_value(type_config, value):
    if value is None:
        return value

    attribute_type = AttributeType(type_config["type"])

    if attribute_type == AttributeType.INT:
        if isinstance(value, float) and value.is_integer():
            return int(value)
        return value

    if attribute_type == AttributeType.LIST:
        return [normalize_typed_value(type_config["args"][0], item) for item in value]

    if attribute_type == AttributeType.SET:
        normalized_items = [normalize_typed_value(type_config["args"][0], item) for item in value]
        return frozenset(canonical_json_value(item) for item in normalized_items)

    if attribute_type == AttributeType.DICT:
        value_type = type_config["args"][-1]
        if len(type_config["args"]) == 1:
            return {key: normalize_typed_value(value_type, item) for key, item in value.items()}

        key_type = type_config["args"][0]
        return {parse_dict_key("attribute", key_type, key): normalize_typed_value(value_type, item) for key, item in value.items()}

    return value


def normalize_attribute_value(attribute_config, value):
    return normalize_typed_value(parse_attribute_type(attribute_config["type"]), value)
