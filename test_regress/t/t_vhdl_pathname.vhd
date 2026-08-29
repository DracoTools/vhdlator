-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_pathname is
end entity;

architecture arch of t_vhdl_pathname is
  signal a : bit;
begin

  blk : block
    signal b : bit;
  begin

    gi : if true generate
      signal c : bit;
    begin
      chk : process is
      begin
        -- The body of a generate is not a named region of its own: the
        -- generate statement's own label is what the path holds (16.2.7)
        assert a'path_name = ":t_vhdl_pathname:a"
          report "a is " & a'path_name severity failure;
        assert b'path_name = ":t_vhdl_pathname:blk:b"
          report "b is " & b'path_name severity failure;
        assert c'path_name = ":t_vhdl_pathname:blk:gi:c"
          report "c is " & c'path_name severity failure;
        assert c'instance_name = ":t_vhdl_pathname(arch):blk:gi:c"
          report "c instance is " & c'instance_name severity failure;

        report "*-* All Finished *-*";
        std.env.finish;
      end process;
    end generate;

  end block;

end architecture;
