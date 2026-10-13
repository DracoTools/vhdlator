-- DESCRIPTION: Verilator: VHDL assertion and report messages
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

-- An assertion without a message reports "Assertion violation." (10.3), and
-- is labelled as one, as GHDL prints it

entity t_vhdl_assertmsg is
end entity;

architecture a of t_vhdl_assertmsg is
begin
  process is
  begin
    report "a report";
    report "a warning" severity warning;
    assert false severity note;
    assert false report "with a message" severity warning;
    assert false;
    assert true report "not printed" severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
