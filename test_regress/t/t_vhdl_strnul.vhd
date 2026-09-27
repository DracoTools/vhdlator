-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- NUL is a CHARACTER like any other (16.3), and UVVM pads every name with it
-- and strips it again: func_cov_pkg's get_bin_name returns "bin_" & idx &
-- fill_string(NUL, n), and string_methods_pkg's to_string drops every
-- control character. A STRING is held as SystemVerilog text, where a 0 byte
-- is no character: one whose length only running settles started out as
-- spaces rather than NULs, writing NUL into one of its elements was ignored,
-- and a NUL character joined to text was dropped. The stripped name came
-- back "bin_1" and fifteen spaces.
entity t_vhdl_strnul is
end entity;

architecture a of t_vhdl_strnul is
  function fill_string(val : character; width : natural) return string is
    variable v_result : string(1 to maximum(1, width));
  begin
    for i in 1 to width loop
      v_result(i) := val;
    end loop;
    return v_result;
  end function;

  -- UVVM's to_string of a string: every control character but LF is dropped
  function strip(val : string) return string is
    variable v_new : string(1 to val'length);
    variable v_idx : natural := 0;
  begin
    for i in val'range loop
      if character'pos(val(i)) >= 32 or val(i) = LF then
        v_idx := v_idx + 1;
        v_new(v_idx) := val(i);
      end if;
    end loop;
    return v_new(1 to v_idx);
  end function;

  function get_bin_name(idx : string) return string is
  begin
    return "bin_" & idx & fill_string(NUL, 20 - 4 - idx'length);
  end function;
begin
  process
    variable n : natural := 3;
    variable c : character := NUL;
    variable v_name : string(1 to 20);
    variable t : string(1 to n + 1);
    -- A length only running settles, so it starts out as NULs
    variable fresh : string(1 to n);
  begin
    assert character'pos(fresh(2)) = 0 severity failure;
    assert character'pos(fill_string(NUL, n)(2)) = 0 severity failure;
    v_name := get_bin_name(integer'image(n - 2));
    assert character'pos(v_name(6)) = 0 severity failure;
    assert strip(v_name) = "bin_1" report "'" & strip(v_name) & "'" severity failure;
    -- A NUL character joined to text is one character long
    t := "ab" & c & 'd';
    assert t'length = 4 and character'pos(t(3)) = 0 severity failure;
    assert t = "ab" & NUL & 'd' severity failure;
    -- And a NUL written over another character replaces it
    t(2) := c;
    assert character'pos(t(2)) = 0 and t(1) = 'a' and t(4) = 'd' severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
