-- this is a comment

INSERT INTO table_name (column1, column2, column3, ...)
VALUES (value1, value2, value3, ...);

BEGIN;
	UPDATE table_name
	SET balance = balance - 100.0
	WHERE account_id = '123';
-- more comment
COMMIT;
