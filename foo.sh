for i in ./*.{cpp,h}; do
    vim -c "set ff=unix" -c "x" $i
done

