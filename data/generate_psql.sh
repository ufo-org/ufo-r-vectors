# Create DB 
sudo -u postgres sh -c "echo \"create user $USER; create database ufos; grant all privileges on database ufos to $USER;\" | psql"

# Create table
echo "drop table if exists example" | psql -U $USER -d ufos
echo "create table example (id serial primary key, integer int, logical boolean, numeric real);" | psql -U $USER -d ufos

# Populate table
for i in {0..2000}
do
    echo -n ${i} "-> ($i, $b, $r) "
    case $(( i % 3 )) in 0) b=TRUE;; 1) b=FALSE;; *) b=NULL;; esac
    r=$(echo "scale=2; $i / 5" | bc)
    echo "insert into example (integer, logical, numeric) values ($i, $b, $r);" | psql -U $USER -d ufos
done

# v <- ufo_psql("dbname = ufos user = $USER", "example", "integer")