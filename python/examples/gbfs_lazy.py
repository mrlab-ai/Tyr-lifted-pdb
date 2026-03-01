
import pytyr.formalism.planning as tfp
import pytyr.planning as tp

from pathlib import Path

ROOT_DIR = (Path(__file__).parent.parent.parent).absolute()

def main():
    domain_filepath = ROOT_DIR / "data" / "gripper" / "domain.pddl"
    task_filepath = ROOT_DIR / "data" / "gripper" / "p-2-0.pddl"

    parser_options = tp.ParserOptions()
    parser = tp.Parser(domain_filepath, parser_options)
    lifted_task = parser.parse_task(task_filepath, parser_options)
    ground_task = lifted_task.instantiate_ground_task()

if __name__ == "__main__":
    main()
