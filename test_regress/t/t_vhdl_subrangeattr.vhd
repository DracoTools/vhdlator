-- DESCRIPTION: Verilator: VHDL bounds of a scalar subtype whose range a generic settles
--
-- "subtype index_type is natural range ram_type'range" with ram_type sized
-- by a generic: index_type'high and 'low are not numbers during analysis,
-- and are the ends of the range as written once the instance is
-- elaborated (16.2.2). They were refused as attributes that cannot be
-- lowered (GHDL synth issue1645).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_subrangeattr is
  generic (size : natural := 4);
end entity;

architecture a of t_vhdl_subrangeattr is
  type ram_type is array (0 to size - 1) of bit_vector(7 downto 0);
  subtype index_type is natural range ram_type'range;
  subtype down_type is integer range size + 2 downto 3;
  signal a : index_type := 0;
begin
  process
  begin
    assert index_type'low = 0 and index_type'high = size - 1 severity failure;
    assert index_type'left = 0 and index_type'right = 3 severity failure;
    assert down_type'high = 6 and down_type'low = 3 severity failure;
    assert down_type'left = 6 and not down_type'ascending severity failure;
    for i in 1 to 6 loop
      if a = index_type'high then
        a <= index_type'low;
      else
        a <= a + 1;
      end if;
      wait for 1 ns;
    end loop;
    assert a = 2 severity failure;
    report "subrangeattr done";
    std.env.finish;
  end process;
end architecture;
