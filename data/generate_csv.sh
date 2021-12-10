# Create a CSV file
echo "integer,logical,numeric,string" > example.csv

# Populate table
strings=( A B C D E F G H I J K L M N O P Q R S T U V X Y Z )
for i in {0..2000}
do
    echo -n ${i} "-> "
    case $(( i % 3 )) in 0) b=TRUE;; 1) b=FALSE;; *) b=NULL;; esac
    r=$(echo "scale=2; $i / 5" | bc)
    s=${strings[$((i % 24))]}
    line="$i,$b,$r,$s"
    echo "$line"
    echo "$line" >> example.csv
done

# v <- ufo_psql("dbname = ufos user = $USER", "example", "integer")