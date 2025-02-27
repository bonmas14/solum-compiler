local syntax = vim.api.nvim_create_augroup("slm_syntax", { clear = true })

vim.api.nvim_create_autocmd("BufRead", {
  group = syntax,
  pattern = "*.slm",
  callback = function()
    -- Define keywords and other syntax elements
    vim.cmd([[
      syntax match slmIdent /[A-Za-z_]\w*/
      syntax keyword slmKeyword ret if else while for module use prototype external struct union enum
      syntax keyword slmType  u8 u16 u32 u64 s8 s16 s32 s64 b32 f32 f64 default

      syntax match slmType /\v[A-Z][a-z0-9]*(_[A-Z][a-z0-9]*)*\v/
    ]])

    vim.cmd([[
        syntax match slmOperator /+/
        syntax match slmOperator /-/
        syntax match slmOperator /\//
        syntax match slmOperator /*/
        syntax match slmOperator /%/

        syntax match slmOperator /&/
        syntax match slmOperator /|/
        syntax match slmOperator /\^/
        syntax match slmOperator /=/
        syntax match slmOperator /;/

        syntax match slmOperator /!/

        syntax match slmOperator /@/
        syntax match slmOperator /\./
        syntax match slmOperator /:/
        syntax match slmOperator /\[/
        syntax match slmOperator /\]/

        syntax match slmOperator /</
        syntax match slmOperator />/

        syntax match slmOperator /&&/
        syntax match slmOperator /||/
        syntax match slmOperator /<</
        syntax match slmOperator />>/

        syntax match slmOperator /<=/
        syntax match slmOperator />=/
        syntax match slmOperator /==/
        syntax match slmOperator /!=/

        syntax match slmOperator /(/
        syntax match slmOperator /)/
        syntax match slmOperator /{/
        syntax match slmOperator /}/
        syntax match slmOperator /\[/
        syntax match slmOperator /\]/

        syntax match slmOperator /->/
    ]])

    vim.cmd([[
      syntax match slmString /".*"/
    ]])

    vim.cmd([[
      syntax match slmComment /\/\/.*/
    ]])

    vim.cmd([[
      syntax region slmMultiComment start=/\/\*/ end=/\*\//
    ]])

    vim.cmd([[
      syntax match slmNumber /\v\d+(\.\d+)?/
      syntax match slmNumber /\v-\d+(\.\d+)?/
      syntax match slmNumber /\v0[x][0-9a-fA-F]+/
      syntax match slmNumber /\v0[b][01]+/
    ]])

    -- Highlights
    vim.cmd([[
      highlight slmKeyword  guifg=#FF7700
      highlight slmOperator guifg=#FF9438
      highlight slmIdent    guifg=#CCCCBB
      highlight slmType     guifg=#FFCC00

      highlight slmComment      guifg=#585858
      highlight slmMultiComment guifg=#585858

      highlight slmString guifg=#66FF44

      highlight slmNumber guifg=#BBFF44
    ]])
  end,
})
