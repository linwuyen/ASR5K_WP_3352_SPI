class TableManager:
    def __init__(self):
        self.tables = {}

    def create_table(self, table_name):
        if table_name in self.tables:
            raise ValueError(f"Table '{table_name}' already exists.")
        self.tables[table_name] = []
        return f"Table '{table_name}' created."

    def delete_table(self, table_name):
        if table_name not in self.tables:
            raise ValueError(f"Table '{table_name}' does not exist.")
        del self.tables[table_name]
        return f"Table '{table_name}' deleted."

    def list_tables(self):
        return list(self.tables.keys()) if self.tables else "No tables available."