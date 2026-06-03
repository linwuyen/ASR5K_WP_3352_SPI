class TableFSM:
    def __init__(self):
        self.states = {}
        self.current_state = None

    def add_state(self, state_name):
        self.states[state_name] = state_name

    def set_state(self, state_name):
        if state_name in self.states:
            self.current_state = state_name
        else:
            raise ValueError(f"State '{state_name}' not found.")

    def get_current_state(self):
        return self.current_state if self.current_state else "No state set."