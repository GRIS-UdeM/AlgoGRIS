#!/usr/bin/env bash
# set -e

script_dir="$(cd -- "$(dirname -- "$0")" && pwd)"

cd "$script_dir"

# Reset SAF code
echo "Resetting SAF code"
cd "$script_dir/../submodules/Spatial_Audio_Framework"
git checkout .

cd "$script_dir"
pwd

TARGET_FILE_SAF_SH_C="../submodules/Spatial_Audio_Framework/framework/modules/saf_sh/saf_sh.c"
TARGET_FILE_BINAURALISER_INTERNAL_C="../submodules/Spatial_Audio_Framework/examples/src/binauraliser/binauraliser_internal.c"
TARGET_FILE_BINAURALISER_NF_C="../submodules/Spatial_Audio_Framework/examples/src/binauraliser_nf/binauraliser_nf.c"

# The new calculateGridWeights
read -r -d '' NEW_BLOCK_SAF_SH_C << 'EOF'
int calculateGridWeights
(
    float* dirs_rad,
    int nDirs,
    int order,
    float* w
)
{
    int i, j, nSH;
    float sumW;
    float **Y_N = NULL, **Y_N_T = NULL, **Y_leftinv = NULL;
    float **Y_tmp = NULL;
    float *YY_N = NULL, *s = NULL;

    if(order<0){
        int ind;
        float minVal, maxVal, cond_N;

        for(int n=1; n<100; n++){
			/* compute the condition number for order N */
            nSH = ORDER2NSH(n);
            Y_tmp = (float**)realloc2d((void**)Y_tmp, nSH, nDirs, sizeof(float));
            YY_N = (float*)realloc1d(YY_N, nSH*nSH*sizeof(float));
            s = (float*)realloc1d(s, nSH*sizeof(float));
            getSHreal(n, dirs_rad, nDirs, FLATTEN2D(Y_tmp));

            cblas_sgemm(CblasRowMajor, CblasNoTrans, CblasTrans, nSH, nSH, nDirs, 1.0f,
                        FLATTEN2D(Y_tmp), nDirs,
                        FLATTEN2D(Y_tmp), nDirs, 0.0f,
                        YY_N, nSH);

			/* condition number = max(singularValues)/min(singularValues) */
            utility_ssvd(NULL, YY_N, nSH, nSH, NULL, NULL, NULL, s);
            utility_simaxv(s, nSH, &ind);
            maxVal = s[ind];
            utility_siminv(s, nSH, &ind);
            minVal = s[ind];
            cond_N = maxVal/(minVal+2.23e-7f);

            if(cond_N > 2 * (n + 1)){  // experimental condition
                order = n-1;
                break;
            }

			/* Hard limit */
            if(n>30){
                order = n-1;
                break;
            }
        }
    }
    if(order<1)  // could not find order
        order=0;

    nSH = ORDER2NSH(order);
    Y_N = (float**)malloc2d(nSH, nDirs, sizeof(float));
    Y_N_T = (float**)malloc2d(nDirs, nSH, sizeof(float));
    Y_leftinv = (float**)malloc2d(nSH, nDirs, sizeof(float));

    getSHreal(order, dirs_rad, nDirs, FLATTEN2D(Y_N));

    for(i=0; i<nDirs; i++)
        for(j=0; j<nSH; j++)
            Y_N_T[i][j] = Y_N[j][i]; /* truncate to current order and transpose */

    utility_spinv(NULL, FLATTEN2D(Y_N_T), nDirs, nSH, FLATTEN2D(Y_leftinv));

    sumW=0.f;
    for(int idx=0; idx<nDirs; idx++){
        w[idx] = sqrtf(FOURPI)*Y_leftinv[0][idx];
        sumW += w[idx];
    }

    if(fabs(sumW - FOURPI) > 0.001f) {
        order=0;
        saf_print_warning("Grid weights no bueno!");
    }

    free(Y_tmp);
    free(YY_N);
    free(s);
    free(Y_N);
    free(Y_N_T);
    free(Y_leftinv);

    return order;
}
EOF

echo "$NEW_BLOCK_SAF_SH_C" > temp_new_function.c

# Execute the line replacement
if sed -i -e '1065,1148d' -e '1064r temp_new_function.c' "$TARGET_FILE_SAF_SH_C"; then
    echo "Success: Replaced lines 1065 to 1148 with the new function calculateGridWeights."
fi

# Clean up the temporary file
rm temp_new_function.c

# echo "$NEW_BLOCK_BINAURALISER_INTERNAL_C" > temp_new_line1.c
printf "\t\t\tfree(hrir_dirs_rad);\n" > temp_new_line1.c
if sed -i -e '379r temp_new_line1.c' "$TARGET_FILE_BINAURALISER_INTERNAL_C"; then
    echo "Success: Adding new line 379 in binauraliser_initHRTFsAndGainTables."
fi

rm temp_new_line1.c

# echo "$NEW_BLOCK_BINAURALISER_NF_C" > temp_new_line2.c
printf "\t\tfree(pData->sofa_filepath);\n" > temp_new_line2.c
if sed -i -e '166r temp_new_line2.c' "$TARGET_FILE_BINAURALISER_NF_C"; then
    echo "Success: Adding new line 166 in binauraliserNF_destroy."
fi

rm temp_new_line2.c
