#!/usr/bin/env bash
set -euo pipefail
OUT="tuner_confirm_top5.csv"
echo "alphaEarly,placementHitMultiplier,adjHitBonus,mcBlendRatio,games,threads,p1_avg_shots,p2_avg_shots" > "$OUT"

candidates=(
"0.800 2.000 0.200 0.500"
"0.700 1.500 0.200 0.000"
"0.750 1.500 0.400 0.500"
"0.800 1.000 0.200 0.500"
"0.650 1.500 0.400 0.500"
)

for c in "${candidates[@]}"; do
  read alpha place adj mc <<< "$c"
  echo "Running: alpha=$alpha place=$place adj=$adj mc=$mc"
  ./tuner games=1000 threads=8 alpha="$alpha" place="$place" adj="$adj" mc="$mc" | tail -n 1 >> "$OUT"
  echo "Appended result to $OUT: $(tail -n 1 $OUT)"
done

echo "All done. Results in $OUT"
