find . -type f -name "*.dl"|while read fname; do
    default=$(~/souffle/src/souffle_default "$fname" --compile --show=transformed-ram | grep "Chains:")
    merged=$(~/souffle/src/souffle_merge "$fname" --compile --show=transformed-ram | grep "Chains:")
    echo "$fname"
    diff <( echo "$default" ) <( echo "$merged" ) 
done
