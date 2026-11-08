"""Unit tests of the transformation table cells and value matching.

Run from tools/gherkin:  python3 -m unittest discover -s tests
"""

import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "features"))

from h2agent_lib import matches, parse_cell, parse_rules, preset_body  # noqa: E402


class Table:
    """Minimal stand-in of a behave table."""

    def __init__(self, headings, rows):
        self.headings = headings
        self.rows = rows

    def __iter__(self):
        return iter(self.rows)


class ParseCell(unittest.TestCase):

    def check(self, cell, discriminator, expected):
        self.assertEqual(parse_cell(cell, discriminator), expected)

    def test_kind_alone(self):
        self.check("recvseq", "source", {"source": "recvseq"})
        self.check("size", "op", {"op": "size"})

    def test_path(self):
        self.check("requestBody /a/b", "source", {"source": "requestBody", "path": "/a/b"})
        self.check("vault cfg /x", "target", {"target": "vault", "key": "cfg", "path": "/x"})

    def test_main_fields_in_order(self):
        self.check("requestHeader authorization", "source",
                   {"source": "requestHeader", "name": "authorization"})
        self.check("regexReplace '^/id-([0-9]+)$' $1", "op",
                   {"op": "regexReplace", "rgx": "^/id-([0-9]+)$", "fmt": "$1"})
        self.check("clientProvision next initial", "target",
                   {"target": "clientProvision", "id": "next", "inState": "initial"})

    def test_named_fields(self):
        self.check("outState method=POST uri=/x", "target",
                   {"target": "outState", "method": "POST", "uri": "/x"})
        self.check("strftime fmt='%Y %m'", "op", {"op": "strftime", "fmt": "%Y %m"})

    def test_types(self):
        self.check("literal 42", "source", {"source": "literal", "value": "42"})  # text
        self.check("random 1 10", "source", {"source": "random", "min": 1, "max": 10})
        self.check("sum 1.5", "op", {"op": "sum", "value": 1.5})
        self.check("split sep=, count=2", "op", {"op": "split", "sep": ",", "count": 2})
        self.check("jsonConstraint '{\"a\": 1}'", "op", {"op": "jsonConstraint", "value": {"a": 1}})
        self.check("equalTo 200 not", "op", {"op": "equalTo", "value": "200", "not": True})

    def test_lists(self):
        self.check("randomset rock paper 'big scissors'", "source",
                   {"source": "randomset", "values": ["rock", "paper", "big scissors"]})

    def test_quotes(self):
        self.check(r"regexCapture '\d+ (\w+)'", "op", {"op": "regexCapture", "rgx": r"\d+ (\w+)"})
        self.check('literal "say \\"hi\\""', "source", {"source": "literal", "value": 'say "hi"'})
        self.check("literal ''", "source", {"source": "literal", "value": ""})

    def test_errors(self):
        for cell, discriminator in [("requestHeader a b", "source"), ("literal 'x", "source"),
                                    ("'quoted' kind", "source"), ("", "source"),
                                    ("sum many", "op"), ("equalTo 1 not=maybe", "op")]:
            with self.assertRaises(AssertionError, msg=cell):
                parse_cell(cell, discriminator)


class ParseRules(unittest.TestCase):

    def test_filter_column_is_optional_and_splits_on_semicolons(self):
        rules = parse_rules(Table(["from", "filter", "to"], [
            ["requestUriPath", "regexReplace 'a;b' x ; cast integer", "var n"],
            ["var n", "", "responseBody /n"]]))
        self.assertEqual(rules, [
            {"from": {"source": "requestUriPath"},
             "filter": [{"op": "regexReplace", "rgx": "a;b", "fmt": "x"}, {"op": "cast", "type": "integer"}],
             "to": {"target": "var", "name": "n"}},
            {"from": {"source": "var", "name": "n"}, "to": {"target": "responseBody", "path": "/n"}}])
        self.assertEqual(parse_rules(Table(["From", "To"], [["recvseq", "var s"]])),
                         [{"from": {"source": "recvseq"}, "to": {"target": "var", "name": "s"}}])

    def test_columns_required(self):
        with self.assertRaises(AssertionError):
            parse_rules(Table(["source", "target"], []))


class Values(unittest.TestCase):

    def test_matches_as_json_or_text(self):
        self.assertTrue(matches("0", "0"))
        self.assertTrue(matches(0, "0"))
        self.assertTrue(matches(True, "true"))
        self.assertTrue(matches({"a": 1}, '{"a": 1}'))
        self.assertTrue(matches(None, "null"))
        self.assertFalse(matches(1, "2"))
        self.assertFalse(matches("1.0", "1"))
        self.assertFalse(matches(True, "True"))

    def test_preset_body(self):
        self.assertEqual(preset_body({"a": 1}, ["p"]), {"#preset": "#{object:p}", "a": 1})
        self.assertEqual(preset_body(None, ["p", "q"]), {"#preset": ["#{object:p}", "#{object:q}"]})
        self.assertEqual(preset_body("text", []), "text")
        with self.assertRaises(AssertionError):
            preset_body("text", ["p"])


if __name__ == "__main__":
    unittest.main()
