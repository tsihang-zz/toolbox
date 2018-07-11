--
-- Create the table children
--
CREATE TABLE children (
    childno int(11) NOT NULL auto_increment,
    fname varchar(30),
    age int(11),
    PRIMARY KEY (childno)
);
--
-- Populate the table 'children'
--
INSERT INTO children(childno, fname, age) VALUES (1, 'Jenny', 21);
INSERT INTO children(childno, fname, age) VALUES (2, 'Andrew', 17);
INSERT INTO children(childno, fname, age) VALUES (3, 'Gavin', 8);
INSERT INTO children(childno, fname, age) VALUES (4, 'Duncan', 6);
INSERT INTO children(childno, fname, age) VALUES (5, 'Emma', 4);
INSERT INTO children(childno, fname, age) VALUES (6, 'Alex', 15);
INSERT INTO children(childno, fname, age) VALUES (7, 'Adrian', 9);

