vim.api.nvim_create_autocmd("FileType", {
  pattern = "slm",
  callback = function()
    vim.cmd[[
      syntax clear
      syntax case match

      syntax match slmIdent /[A-Za-z_]\w*/

      " Keywords
      syntax keyword slmKeyword true false return if then else while for default cast break continue use external struct union enum as
      syntax keyword slmType u8 u16 u32 u64 s8 s16 s32 s64 b8 b32 f32 f64 void
      syntax match slmType /\v<[A-Z]\w*(\.[A-Z]\w*)*>/

      " Operators
      syntax match slmOperator /\(<<\|>>\|<=\|>=\|==\|!=\|&&\|||\)/
      syntax match slmOperator /[+\-*\/%&|\^=<>!;.,:@()[\]{}]/
      syntax match slmOperator /->/

      " Literals
      syntax match slmNumber /\v<-?\d+(\.\d+)?([eE][+-]?\d+)?>/
      syntax match slmNumber /\v<0x\x+>/
      syntax match slmNumber /\v<0b[01]+>/
      syntax keyword slmBoolean true false
      syntax region slmString start=/"/ skip=/\\"/ end=/"/

      " Comments
      syntax match slmComment /\/\/.*/
      syntax region slmMultiComment start=/\/\*/ end=/\*\// contains=slmMultiComment

      " Highlight links
      highlight link slmIdent Identifier
      highlight link slmKeyword Keyword
      highlight link slmType Type
      highlight link slmOperator Operator
      highlight link slmString String
      highlight link slmNumber Number
      highlight link slmBoolean Boolean
      highlight link slmComment Comment
    ]]
  end
})
